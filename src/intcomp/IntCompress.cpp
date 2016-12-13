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
#include "intcomp/AbbreviationsCollector.h"
#include "intcomp/CountWriter.h"
#include "intcomp/RemoveNodesVisitor.h"
#include "interp/IntReader.h"
#include "interp/IntWriter.h"
#include "interp/StreamReader.h"
#include "sexp/TextWriter.h"
#include "utils/heap.h"
#include "utils/circular-vector.h"

#include <algorithm>
#include <vector>
#include <set>

#define DESCRIBE_INPUT 0

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
  std::shared_ptr<SymbolTable> OutSymtab = std::make_shared<SymbolTable>();
  generateCode(OutSymtab, AbbrevAssignments);
  writeOutput(OutSymtab);
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

namespace {

Node* generateAbbrevFormat(std::shared_ptr<SymbolTable> Symtab,
                           IntTypeFormat AbbrevFormat) {
  switch (AbbrevFormat) {
    case IntTypeFormat::Uint8:
      return Symtab->getUint8Definition();
    case IntTypeFormat::Varint32:
      return Symtab->getVarint32Definition();
    case IntTypeFormat::Varuint32:
      return Symtab->getVaruint32Definition();
    case IntTypeFormat::Uint32:
      return Symtab->getUint32Definition();
    case IntTypeFormat::Varint64:
      return Symtab->getVarint64Definition();
    case IntTypeFormat::Varuint64:
      return Symtab->getVaruint64Definition();
    case IntTypeFormat::Uint64:
      return Symtab->getUint64Definition();
  }
  WASM_RETURN_UNREACHABLE(nullptr);
}

Node* generateIntValue(std::shared_ptr<SymbolTable> Symtab, IntType Value) {
  return Symtab->getU64ConstDefinition(Value, decode::ValueFormat::Decimal);
}

Node* generateDefaultSingleAction(std::shared_ptr<SymbolTable> Symtab) {
  return Symtab->getVarint64Definition();
}

Node* generateDefaultMultipleAction(std::shared_ptr<SymbolTable> Symtab) {
  return Symtab->create<LoopNode>(Symtab->getVaruint64Definition(),
                                  generateDefaultSingleAction(Symtab));
}

Node* generateDefaultAction(std::shared_ptr<SymbolTable> Symtab,
                            DefaultCountNode* Default) {
  return Default->isSingle() ? generateDefaultSingleAction(Symtab)
                             : generateDefaultMultipleAction(Symtab);
}

Node* generateIntLitAction(std::shared_ptr<SymbolTable> Symtab,
                           IntCountNode* Nd) {
  if (Nd->getPathLength() == 1)
    return generateIntValue(Symtab, Nd->getValue());
  std::vector<IntCountNode*> Values;
  while (Nd) {
    Values.push_back(Nd);
    Nd = Nd->getParent().get();
  }
  auto* Seq = Symtab->create<SequenceNode>();
  for (IntCountNode* Nd : Values)
    Seq->append(generateIntValue(Symtab, Nd->getValue()));
  return Seq;
}

Node* generateBlockAction(std::shared_ptr<SymbolTable> Symtab,
                          BlockCountNode* Blk) {
  return Symtab->create<CallbackNode>(
      Symtab->getPredefined(Blk->isEnter() ? PredefinedSymbol::Block_enter
                                           : PredefinedSymbol::Block_exit));
}

Node* generateAction(std::shared_ptr<SymbolTable> Symtab, CountNode::Ptr Nd) {
  CountNode* NdPtr = Nd.get();
  if (auto* CntNd = dyn_cast<IntCountNode>(NdPtr))
    return generateIntLitAction(Symtab, CntNd);
  else if (auto* BlkPtr = dyn_cast<BlockCountNode>(NdPtr))
    return generateBlockAction(Symtab, BlkPtr);
  else if (auto* DefaultPtr = dyn_cast<DefaultCountNode>(NdPtr))
    return generateDefaultAction(Symtab, DefaultPtr);
  return Symtab->create<ErrorNode>();
}

Node* generateCase(std::shared_ptr<SymbolTable> Symtab,
                   size_t AbbrevIndex,
                   CountNode::Ptr Nd) {
  return Symtab->create<CaseNode>(
      Symtab->getU64ConstDefinition(AbbrevIndex, decode::ValueFormat::Decimal),
      generateAction(Symtab, Nd));
}

Node* generateSwitchStatement(std::shared_ptr<SymbolTable> Symtab,
                              CountNode::RootPtr Root,
                              IntTypeFormat AbbrevFormat,
                              CountNode::PtrVector& Assignments) {
  auto SwitchStmt = Symtab->create<SwitchNode>();
  SwitchStmt->append(generateAbbrevFormat(Symtab, AbbrevFormat));
  SwitchStmt->append(Symtab->create<ErrorNode>());
  for (size_t i = 0; i < Assignments.size(); ++i)
    SwitchStmt->append(generateCase(Symtab, i, Assignments[i]));
  return SwitchStmt;
}

Node* generateFileFcn(std::shared_ptr<SymbolTable> Symtab,
                      CountNode::RootPtr Root,
                      IntTypeFormat AbbrevFormat,
                      CountNode::PtrVector& Assignments) {
  auto Fcn = Symtab->create<DefineNode>();
  Fcn->append(Symtab->getPredefined(PredefinedSymbol::Section));
  Fcn->append(Symtab->getParamsDefinition());
  Fcn->append(generateSwitchStatement(Symtab, Root, AbbrevFormat, Assignments));
  return Fcn;
}

Node* generateFileBody(std::shared_ptr<SymbolTable> Symtab,
                       CountNode::RootPtr Root,
                       IntTypeFormat AbbrevFormat,
                       CountNode::PtrVector& Assignments) {
  auto Body = Symtab->create<SectionNode>();
  Body->append(generateFileFcn(Symtab, Root, AbbrevFormat, Assignments));
  return Body;
}

}  // End of anonymous namespace

void IntCompressor::generateCode(std::shared_ptr<SymbolTable> Symtab,
                                 CountNode::PtrVector& Assignments,
                                 bool Trace) {
  TRACE_METHOD("generateCode");
  auto File = Symtab->create<FileNode>();
  File->append(Symtab->getWasmVersionDefinition(
      CasmBinaryMagic, decode::ValueFormat::Hexidecimal));
  File->append(Symtab->getCasmVersionDefinition(
      CasmBinaryVersion, decode::ValueFormat::Hexidecimal));
  File->append(generateFileBody(Symtab, Root, AbbrevFormat, Assignments));
  if (Trace) {
    TRACE_MESSAGE("Compressed code:");
    getTrace().getTextWriter()->write(getTrace().getFile(), File);
  }
  Symtab->install(File);
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
