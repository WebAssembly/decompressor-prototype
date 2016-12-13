// -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implements the compressor of WASM files base on integer usage.

#include "intcomp/IntCompress.h"

#include "intcomp/AbbrevAssignWriter.h"
#include "intcomp/AbbreviationCodegen.h"
#include "intcomp/AbbreviationsCollector.h"
#include "intcomp/CountWriter.h"
#include "intcomp/RemoveNodesVisitor.h"
#include "interp/IntReader.h"
#include "sexp/TextWriter.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace intcomp {

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Input(Input),
      Output(Output),
      Symtab(Symtab),
      CountCutoff(0),
      WeightCutoff(0),
      LengthLimit(1),
      AbbrevFormat(IntTypeFormat::Varuint64),
      ErrorsFound(false) {
}

void IntCompressor::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClassSexp> IntCompressor::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("IntCompress"));
  return Trace;
}

CountNode::RootPtr IntCompressor::getRoot() {
  if (!Root)
    Root = std::make_shared<RootCountNode>();
  return Root;
}

void IntCompressor::readInput(bool TraceParsing) {
  Contents = std::make_shared<IntStream>();
  IntWriter MyWriter(Contents);
  StreamReader MyReader(Input, MyWriter, Symtab);
  if (TraceParsing) {
    TraceClassSexp& Trace = MyReader.getTrace();
    Trace.addContext(MyReader.getTraceContext());
    Trace.addContext(MyWriter.getTraceContext());
    Trace.setTraceProgress(true);
  }
  MyReader.fastStart();
  MyReader.fastReadBackFilled();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  Input.reset();
  return;
}

void IntCompressor::writeOutput(std::shared_ptr<SymbolTable> Symtab,
                                bool Trace) {
  TRACE_METHOD("writeOutput");
  StreamWriter MyWriter(Output);
  IntReader MyReader(IntOutput, MyWriter, Symtab);
  if (Trace) {
    TRACE(bool, "Trace", Trace);
    TraceClassSexp& Trace = MyReader.getTrace();
    Trace.addContext(MyReader.getTraceContext());
    Trace.addContext(MyWriter.getTraceContext());
    Trace.setTraceProgress(true);
  }
  MyReader.insertFileVersion(WasmBinaryMagic, WasmBinaryVersionD);
  MyReader.algorithmStart();
  MyReader.algorithmReadBackFilled();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  IntOutput.reset();
  return;
}

IntCompressor::~IntCompressor() {
}

bool IntCompressor::compressUpToSize(size_t Size, bool TraceParsing) {
  TRACE_BLOCK({
    if (Size == 1)
      TRACE_MESSAGE("Collecting integer sequences of length: 1");
    else
      TRACE_MESSAGE("Collecting integer sequences of (up to) length: " +
                    std::to_string(Size));
  });
  CountWriter Writer(getRoot());
  Writer.setCountCutoff(CountCutoff);
  Writer.setUpToSize(Size);
  IntReader Reader(Contents, Writer, Symtab);
  if (TraceParsing)
    Reader.getTrace().setTraceProgress(true);
  Reader.fastStart();
  Reader.fastReadBackFilled();
  return !Reader.errorsFound();
}

void IntCompressor::removeSmallUsageCounts() {
  // NOTE: The main purpose of this method is to shrink the size of
  // the trie to (a) recover memory and (b) make remaining analysis
  // faster.  It does this by removing int count nodes that are not
  // not useful (See case RemoveFrame::State::Exit for details).
  RemoveNodesVisitor Visitor(Root, CountCutoff);
  Visitor.walk();
}

void IntCompressor::compress(DetailLevel Level,
                             bool TraceParsing,
                             bool TraceFirstPassOnly) {
  TRACE_METHOD("compress");
  readInput(TraceParsing);
  if (errorsFound()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  TRACE(size_t, "Number of integers in input", Contents->getNumIntegers());
  if (Level == DetailLevel::AllDetail)
    Contents->describe(stderr, "Input int stream");
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // trie.
  if (!compressUpToSize(1, TraceParsing && !TraceFirstPassOnly))
    return;
  if (Level >= DetailLevel::MoreDetail)
    describe(stderr, makeFlags(CollectionFlag::TopLevel));
  removeSmallUsageCounts();
  if (LengthLimit > 1) {
    if (!compressUpToSize(LengthLimit, TraceParsing && !TraceFirstPassOnly))
      return;
    removeSmallUsageCounts();
  }
  if (Level >= DetailLevel::MoreDetail)
    describe(stderr, makeFlags(CollectionFlag::IntPaths));
  CountNode::PtrVector AbbrevAssignments;
  assignInitialAbbreviations(AbbrevAssignments);
  if (Level >= DetailLevel::MoreDetail)
    describe(stderr, makeFlags(CollectionFlag::All));
  IntOutput = std::make_shared<IntStream>();
  if (!generateIntOutput(TraceParsing && !TraceFirstPassOnly))
    return;
  TRACE(size_t, "Number of integers in compressed output",
        IntOutput->getNumIntegers());
  if (Level >= DetailLevel::MoreDetail)
    IntOutput->describe(stderr, "Input int stream");
#if 0
  std::shared_ptr<SymbolTable> OutSymtab =
      generateCode(AbbrevAssignments, true, true);
  writeOutput(OutSymtab, true);
#else
  std::shared_ptr<SymbolTable> OutSymtab =
      generateCode(AbbrevAssignments, false);
  writeOutput(OutSymtab);
#endif
  if (errorsFound()) {
    fprintf(stderr, "Unable to compress, output malformed\n");
    return;
  }
}

void IntCompressor::assignInitialAbbreviations(
    CountNode::PtrVector& Assignments) {
  AbbreviationsCollector Collector(getRoot(), AbbrevFormat, Assignments);
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.assignAbbreviations();
}

bool IntCompressor::generateIntOutput(bool TraceParsing) {
  AbbrevAssignWriter Writer(Root, IntOutput, LengthLimit, AbbrevFormat);
  IntReader Reader(Contents, Writer, Symtab);
  if (TraceParsing) {
    std::shared_ptr<TraceClassSexp> Trace = Writer.getTracePtr();
    Reader.setTrace(Trace);
    Writer.setTrace(Trace);
    Reader.getTrace().setTraceProgress(true);
    Writer.getTrace().setTraceProgress(true);
  }
  Reader.fastStart();
  Reader.fastReadBackFilled();
  assert(IntOutput->isFrozen());
  return !Reader.errorsFound();
}

std::shared_ptr<SymbolTable> IntCompressor::generateCode(
    CountNode::PtrVector& Assignments,
    bool ToRead,
    bool Trace) {
  AbbreviationCodegen Codegen(Root, AbbrevFormat, Assignments);
  std::shared_ptr<SymbolTable> Symtab = Codegen.getCodeSymtab(ToRead);
  if (Trace)
    getTrace().getTextWriter()->write(getTrace().getFile(),
                                      Symtab->getInstalledRoot());
  return Symtab;
}

void IntCompressor::describe(FILE* Out, CollectionFlags Flags) {
  CountNodeCollector Collector(getRoot());
  if (hasTrace())
    Collector.setTrace(getTracePtr());
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.collect(Flags);
  TRACE_BLOCK({ Collector.describe(getTrace().getFile()); });
}

}  // end of namespace intcomp

}  // end of namespace wasm
