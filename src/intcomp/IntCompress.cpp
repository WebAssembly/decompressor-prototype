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
#include "sexp/CasmWriter.h"
#include "sexp/TextWriter.h"
#include "utils/ArgsParse.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;
using namespace wasm;

namespace {

charstring CollectionFlagsName[] = {"None", "TopLevel", "IntPaths", "All"};

}  // end of anonymous namespace

namespace intcomp {

charstring getName(CollectionFlags Flags) {
  if (Flags < size(CollectionFlagsName))
    return CollectionFlagsName[Flags];
  return "UnknownCollectionFlags";
}

IntCompressor::Flags::Flags()
    : CountCutoff(0),
      WeightCutoff(0),
      LengthLimit(1),
      AbbrevFormat(IntTypeFormat::Varuint64),
      MinimizeCodeSize(true),
      TraceReadingInput(false),
      TraceReadingIntStream(false),
      TraceWritingCodeOutput(false),
      TraceWritingDataOutput(false),
      TraceCompression(false),
      TraceIntStreamGeneration(false),
      TraceCodeGenerationForReading(false),
      TraceCodeGenerationForWriting(false),
      TraceInputIntStream(false),
      TraceIntCounts(false),
      TraceSequenceCounts(false),
      TraceAbbreviationAssignments(false),
      TraceCompressedIntOutput(false) {
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab,
                             Flags& MyFlags)
    : Input(Input),
      Output(Output),
      MyFlags(MyFlags),
      Symtab(Symtab),
      ErrorsFound(false) {
  if (MyFlags.TraceCompression)
    setTraceProgress(true);
}

void IntCompressor::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  MyFlags.Trace = NewTrace;
}

std::shared_ptr<TraceClass> IntCompressor::getTracePtr() {
  if (!MyFlags.Trace)
    setTrace(std::make_shared<TraceClass>("IntCompress"));
  return MyFlags.Trace;
}

CountNode::RootPtr IntCompressor::getRoot() {
  if (!Root)
    Root = std::make_shared<RootCountNode>();
  return Root;
}

void IntCompressor::readInput() {
  Contents = std::make_shared<IntStream>();
  IntWriter MyWriter(Contents);
  StreamReader MyReader(Input, MyWriter, Symtab);
  if (MyFlags.TraceReadingInput)
    MyReader.getTrace().setTraceProgress(true);
  MyReader.fastStart();
  MyReader.fastReadBackFilled();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  Input.reset();
  return;
}

const WriteCursor IntCompressor::writeCodeOutput(
    std::shared_ptr<SymbolTable> Symtab) {
  CasmWriter Writer;
  return Writer.setTraceWriter(MyFlags.TraceWritingCodeOutput)
      .setTraceTree(MyFlags.TraceWritingCodeOutput)
      .setMinimizeBlockSize(MyFlags.MinimizeCodeSize)
      .setFreezeEofAtExit(false)
      .writeBinary(Symtab, Output);
}

void IntCompressor::writeDataOutput(const WriteCursor& StartPos,
                                    std::shared_ptr<SymbolTable> Symtab) {
  StreamWriter Writer(Output);
  Writer.setPos(StartPos);
  IntReader Reader(IntOutput, Writer, Symtab);
  if (MyFlags.TraceWritingDataOutput)
    Reader.getTrace().setTraceProgress(true);
  Reader.useFileHeader(Symtab->getTargetHeader());
  Reader.algorithmStart();
  Reader.algorithmReadBackFilled();
  bool Successful = Reader.isFinished() && Reader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  return;
}

IntCompressor::~IntCompressor() {
}

bool IntCompressor::compressUpToSize(size_t Size) {
  TRACE_BLOCK({
    if (Size == 1)
      TRACE_MESSAGE("Collecting integer sequences of length: 1");
    else
      TRACE_MESSAGE("Collecting integer sequences of (up to) length: " +
                    std::to_string(Size));
  });
  CountWriter Writer(getRoot());
  Writer.setCountCutoff(MyFlags.CountCutoff);
  Writer.setUpToSize(Size);

  IntReader Reader(Contents, Writer, Symtab);
  if (MyFlags.TraceReadingIntStream)
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
  RemoveNodesVisitor Visitor(Root, MyFlags.CountCutoff);
  Visitor.walk();
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
  TRACE_MESSAGE("Reading input");
  readInput();
  if (errorsFound()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  TRACE(size_t, "Number of integers in input", Contents->getNumIntegers());
  if (MyFlags.TraceInputIntStream)
    Contents->describe(stderr, "Input int stream");
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // trie.
  if (!compressUpToSize(1))
    return;
  if (MyFlags.TraceIntCounts)
    describe(stderr, makeFlags(CollectionFlag::TopLevel));
  removeSmallUsageCounts();
  if (MyFlags.LengthLimit > 1) {
    if (!compressUpToSize(MyFlags.LengthLimit))
      return;
    removeSmallUsageCounts();
  }
  if (MyFlags.TraceSequenceCounts)
    describe(stderr, makeFlags(CollectionFlag::IntPaths));
  TRACE_MESSAGE("Assigning (initial) abbreviations to integer sequences");
  CountNode::PtrVector AbbrevAssignments;
  assignInitialAbbreviations(AbbrevAssignments);
  if (MyFlags.TraceAbbreviationAssignments)
    describe(stderr, makeFlags(CollectionFlag::All));
  IntOutput = std::make_shared<IntStream>();
  TRACE_MESSAGE("Generating compressed integer stream");
  if (!generateIntOutput())
    return;
  TRACE(size_t, "Number of integers in compressed output",
        IntOutput->getNumIntegers());
  if (MyFlags.TraceCompressedIntOutput)
    IntOutput->describe(stderr, "Output int stream");
  TRACE_MESSAGE("Appending compression algorithm to output");
  const WriteCursor Pos =
      writeCodeOutput(generateCodeForReading(AbbrevAssignments));
  if (errorsFound()) {
    fprintf(stderr, "Unable to compress, output malformed\n");
    return;
  }
  TRACE_MESSAGE("Appending compressed WASM file to output");
  writeDataOutput(Pos, generateCodeForWriting(AbbrevAssignments));
  if (errorsFound()) {
    fprintf(stderr, "Unable to compress, output malformed\n");
    return;
  }
}

void IntCompressor::assignInitialAbbreviations(
    CountNode::PtrVector& Assignments) {
  AbbreviationsCollector Collector(getRoot(), MyFlags.AbbrevFormat,
                                   Assignments);
  Collector.CountCutoff = MyFlags.CountCutoff;
  Collector.WeightCutoff = MyFlags.WeightCutoff;
  Collector.assignAbbreviations();
}

bool IntCompressor::generateIntOutput() {
  AbbrevAssignWriter Writer(Root, IntOutput, MyFlags.LengthLimit,
                            MyFlags.AbbrevFormat);
  IntReader Reader(Contents, Writer, Symtab);
  if (MyFlags.TraceIntStreamGeneration)
    Reader.getTrace().setTraceProgress(true);
  Reader.fastStart();
  Reader.fastReadBackFilled();
  assert(IntOutput->isFrozen());
  return !Reader.errorsFound();
}

std::shared_ptr<SymbolTable> IntCompressor::generateCode(
    CountNode::PtrVector& Assignments,
    bool ToRead,
    bool Trace) {
  AbbreviationCodegen Codegen(Root, MyFlags.AbbrevFormat, Assignments);
  std::shared_ptr<SymbolTable> Symtab = Codegen.getCodeSymtab(ToRead);
  if (Trace) {
    TextWriter Writer;
    Writer.write(stderr, Symtab->getInstalledRoot());
  }
  return Symtab;
}

void IntCompressor::describe(FILE* Out, CollectionFlags Flags) {
  CountNodeCollector Collector(getRoot());
  if (hasTrace())
    Collector.setTrace(getTracePtr());
  Collector.CountCutoff = MyFlags.CountCutoff;
  Collector.WeightCutoff = MyFlags.WeightCutoff;
  Collector.collect(Flags);
  TRACE_BLOCK({ Collector.describe(getTrace().getFile()); });
}

}  // end of namespace intcomp

}  // end of namespace wasm
