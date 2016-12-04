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

// Implements the compressor of WASM fiels base on integer usage.

#include "intcomp/IntCompress.h"
#include "interp/IntReader.h"
#include "interp/IntWriter.h"
#include "interp/StreamReader.h"
#include "utils/circular-vector.h"
#include "utils/heap.h"

#include <vector>
#include <algorithm>

#define DESCRIBE_INPUT 0

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace intcomp {

class IntCounterWriter : public Writer {
  IntCounterWriter() = delete;
  IntCounterWriter(const IntCounterWriter&) = delete;
  IntCounterWriter& operator=(const IntCounterWriter&) = delete;

 public:
  IntCounterWriter(CountNode::RootPtr Root)
      : Root(Root),
        input_seq(new circular_vector<IntType>(1)),
        CountCutoff(1),
        WeightCutoff(1),
        UpToSize(0) {}

  ~IntCounterWriter() OVERRIDE;

  void setSequenceSize(size_t Size);
  void setCountCutoff(uint64_t NewValue) { CountCutoff = NewValue; }
  void setWeightCutoff(uint64_t NewValue) { WeightCutoff = NewValue; }
  void setUpToSize(size_t NewSize) {
    UpToSize = NewSize;
    setSequenceSize(NewSize);
  }
  void resetUpToSize() {
    UpToSize = 0;
    setSequenceSize(UpToSize);
  }
  size_t getUpToSize() const { return UpToSize; }

  void addToUsageMap(IntType Value);
  void addInputSeqToUsageMap();
  void addAllInputSeqsToUsageMap();

  StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

 private:
  CountNode::RootPtr Root;
  circular_vector<IntType>* input_seq;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  size_t UpToSize;
  void popValuesFromInputSeq(size_t Size);
};

IntCounterWriter::~IntCounterWriter() {
  // TODO(karlschimpf): Recover memory of input_seq. Not done now due to
  // (optimized) of destruction of circular_vector<IntType>
}

StreamType IntCounterWriter::getStreamType() const {
  return StreamType::Int;
}

void IntCounterWriter::setSequenceSize(size_t Size) {
  input_seq->clear();
  if (Size > 0)
    input_seq->resize(Size);
}

void IntCounterWriter::popValuesFromInputSeq(size_t Size) {
  for (size_t i = 0; i < Size; ++i) {
    if (input_seq->empty())
      return;
    input_seq->pop_front();
  }
}

void IntCounterWriter::addInputSeqToUsageMap() {
  if (input_seq->empty())
    return;
  CountNode::IntPtr Nd;
  for (size_t i = 0, e = input_seq->size(); i < e; ++i) {
    IntType Val = (*input_seq)[i];
    Nd = (i == 0) ? lookup(Root, Val) : lookup(Nd, Val);
    if (UpToSize == 1 || i > 0)
      Nd->increment();
    if (e > 1) {
      if (Nd->getCount() < CountCutoff || Nd->getWeight() < WeightCutoff) {
        popValuesFromInputSeq(i == 0 ? 1 : i);
        return;
      }
    }
    if (i + 1 == e) {
      popValuesFromInputSeq(e);
      return;
    }
  }
  // If reached, no values added. Pop one value so that this
  // metod guarantees to shrink the input sequence.
  popValuesFromInputSeq(1);
}

void IntCounterWriter::addAllInputSeqsToUsageMap() {
  while (!input_seq->empty())
    addInputSeqToUsageMap();
}

void IntCounterWriter::addToUsageMap(IntType Value) {
  input_seq->push_back(Value);
  if (!input_seq->full())
    return;
  addInputSeqToUsageMap();
}

bool IntCounterWriter::writeUint8(uint8_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeUint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeUint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVarint32(int32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVarint64(int64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVaruint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVaruint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeValue(decode::IntType Value, const filt::Node*) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
      addAllInputSeqsToUsageMap();
      Root->getBlockEnter()->increment();
      return true;
    case PredefinedSymbol::Block_exit:
      Root->getBlockExit()->increment();
      addAllInputSeqsToUsageMap();
      return true;
    default:
      return true;
  }
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Input(Input),
      Output(Output),
      Symtab(Symtab),
      CountCutoff(0),
      WeightCutoff(0),
      LengthLimit(1),
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

void IntCompressor::readInput(std::shared_ptr<Queue> InputStream,
                              std::shared_ptr<SymbolTable> Symtab,
                              bool TraceParsing) {
  Contents = std::make_shared<IntStream>();
  IntWriter MyWriter(Contents);
  StreamReader MyReader(InputStream, MyWriter, Symtab);
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
  IntCounterWriter Writer(getRoot());
  Writer.setCountCutoff(CountCutoff);
  Writer.setWeightCutoff(WeightCutoff);
  Writer.setUpToSize(Size);
  IntReader Reader(Contents, Writer, Symtab);
  Reader.getTrace().setTraceProgress(TraceParsing);
  Reader.fastStart();
  Reader.fastReadBackFilled();
  if (!Reader.errorsFound())
    Writer.addAllInputSeqsToUsageMap();
  return !Reader.errorsFound();
}

bool IntCompressor::removeSmallUsageCounts(CountNode::Ptr Nd) {
  assert(Nd);
  bool IsRemovable = true;
  if (Nd->getCount() >= CountCutoff || Nd->getWeight() >= WeightCutoff)
    IsRemovable = false;
  if (auto* SuccNd = dyn_cast<CountNodeWithSuccs>(Nd.get())) {
    std::vector<IntType> KeysToRemove;
    for (CountNode::SuccMapIterator Iter = SuccNd->getSuccBegin(),
                                    End = SuccNd->getSuccEnd();
         Iter != End; ++Iter) {
      bool KeepKey = true;
      if (!Iter->second)
        KeepKey = false;
      else if (removeSmallUsageCounts(Iter->second))
        KeepKey = false;
      if (!KeepKey)
        KeysToRemove.push_back(Iter->first);
    }
    for (const auto Key : KeysToRemove)
      SuccNd->eraseSucc(Key);
    if (SuccNd->hasSuccessors())
      IsRemovable = false;
  }
  return IsRemovable;
}

void IntCompressor::compress(DetailLevel Level,
                             bool TraceParsing,
                             bool TraceFirstPassOnly) {
  TRACE_METHOD("compress");
  readInput(Input, Symtab, TraceParsing);
  if (errorsFound()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // IntCountNode trie.
  if (!compressUpToSize(1, TraceParsing && !TraceFirstPassOnly))
    return;
  if (Level == AllDetail)
    describe(stderr, makeFlags(CollectionFlag::TopLevel));
  if (LengthLimit > 1) {
    if (!compressUpToSize(LengthLimit, TraceParsing && !TraceFirstPassOnly))
      return;
    removeSmallUsageCounts();
  }
  if (Level == AllDetail)
    describe(stderr, makeFlags(CollectionFlag::IntPaths));
  assignInitialAbbreviations();
  if (Level != DetailLevel::NoDetail)
    describe(stderr, makeFlags(CollectionFlag::All));
}

namespace {

class CountNodeCollector {
 public:
  CountNode::RootPtr Root;
  std::vector<CountNode::HeapValueType> Values;
  std::shared_ptr<CountNode::HeapType> ValuesHeap;
  uint64_t WeightTotal;
  uint64_t CountTotal;
  uint64_t WeightReported;
  uint64_t CountReported;
  uint64_t NumNodesReported;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  std::shared_ptr<filt::TraceClassSexp> Trace;

  explicit CountNodeCollector(CountNode::RootPtr Root)
      : Root(Root),
        ValuesHeap(std::make_shared<CountNode::HeapType>()),
        WeightTotal(0),
        CountTotal(0),
        WeightReported(0),
        CountReported(0),
        NumNodesReported(0),
        CountCutoff(1),
        WeightCutoff(1) {}
  ~CountNodeCollector() { clear(); }

  void collect(CollectionFlags Flags = makeFlags(CollectionFlag::All));
  void buildHeap();
  CountNode::HeapValueType popHeap() {
    assert(ValuesHeap);
    CountNode::HeapEntryType Entry = ValuesHeap->top();
    ValuesHeap->pop();
    return Entry->getValue();
  }
  void clearHeap();
  void describeHeap(FILE* Out) {
    ValuesHeap->describe(Out, [](FILE* Out, CountNode::HeapValueType Value) {
      Value->describe(Out);
    });
  }
  void clear();
  void collectNode(CountNode::Ptr Nd, CollectionFlags Flags);
  void assignInitialAbbreviations();

  void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace);
  filt::TraceClassSexp& getTrace();
  bool hasTrace() { return bool(Trace); }
  void describe(FILE* Out);
};

void CountNodeCollector::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
}

filt::TraceClassSexp& CountNodeCollector::getTrace() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("IntCompress"));
  return *Trace;
}

void CountNodeCollector::clearHeap() {
  for (auto& Value : Values)
    Value->disassociateFromHeap();
}

void CountNodeCollector::clear() {
  clearHeap();
  Values.clear();
  ValuesHeap.reset();
}

void CountNodeCollector::buildHeap() {
  for (auto& Value : Values)
    Value->associateWithHeap(ValuesHeap->push(Value));
}

void CountNodeCollector::collect(CollectionFlags Flags) {
  if (hasFlag(CollectionFlag::TopLevel, Flags)) {
    collectNode(Root->getBlockEnter(), Flags);
    collectNode(Root->getBlockExit(), Flags);
  }
  for (CountNode::SuccMapIterator Iter = Root->getSuccBegin(),
                                  End = Root->getSuccEnd();
       Iter != End; ++Iter)
    collectNode(Iter->second, Flags);
}

void CountNodeCollector::collectNode(CountNode::Ptr Nd, CollectionFlags Flags) {
  std::vector<CountNode::Ptr> ToAdd;
  ToAdd.push_back(Nd);
  while (!ToAdd.empty()) {
    Nd = ToAdd.back();
    ToAdd.pop_back();
    if (!Nd)  // This shouldn't happen, but be safe.
      continue;

    if (auto* IntNd = dyn_cast<IntCountNode>(*Nd))
      for (CountNode::SuccMapIterator Iter = IntNd->getSuccBegin(),
                                      End = IntNd->getSuccEnd();
           Iter != End; ++Iter)
        ToAdd.push_back(Iter->second);

    uint64_t Weight = Nd->getWeight();
    uint64_t Count = Nd->getCount();
    bool IsSingleton = isa<SingletonCountNode>(*Nd);
    auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
    if (hasFlag(CollectionFlag::TopLevel, Flags)) {
      CountTotal += Count;
      WeightTotal += Weight;
      if (Count < CountCutoff)
        continue;
      if (Weight < WeightCutoff)
        continue;
      if (IntNd == nullptr || IsSingleton) {
        CountReported += Count;
        WeightReported += Weight;
        ++NumNodesReported;
        Values.push_back(Nd);
      }
    }
    if (IntNd == nullptr || !hasFlag(CollectionFlag::IntPaths, Flags))
      continue;
    if (!IsSingleton) {
      CountTotal += Count;
      WeightTotal += Weight;
    }
    if (Count < CountCutoff)
      continue;
    if (Weight < WeightCutoff)
      continue;
    if (!IsSingleton) {
      Values.push_back(CountNode::Ptr(Nd));
      CountReported += Count;
      WeightReported += Weight;
      ++NumNodesReported;
    }
  }
}

void CountNodeCollector::assignInitialAbbreviations() {
  collect(makeFlags(CollectionFlag::All));
  buildHeap();
  IntType AbbrevIndex = 0;
  while (!ValuesHeap->empty()) {
    CountNode::HeapValueType NdPtr = popHeap();
    NdPtr->setAbbrevIndex(AbbrevIndex++);
    // TODO(karlschimpf): Fix related nodes (prefix/suffix).
  }
}

void CountNodeCollector::describe(FILE* Out) {
  assert(ValuesHeap->empty());
  buildHeap();
  fprintf(Out, "Number nodes reported: %" PRIuMAX
               "\n"
               "Total weight: %" PRIuMAX " Reported Weight %" PRIuMAX
               "\n"
               "Total count: %" PRIuMAX " Reported count %" PRIuMAX "\n",
          uintmax_t(NumNodesReported), uintmax_t(WeightTotal),
          uintmax_t(WeightReported), uintmax_t(CountTotal),
          uintmax_t(CountReported));
  size_t Count = 0;
  while (!ValuesHeap->empty()) {
    CountNode::HeapValueType NdPtr = popHeap();
    ++Count;
    fprintf(Out, "%8" PRIuMAX ": ", uintmax_t(Count));
    NdPtr->describe(Out);
  }
}

}  // end of anonymous namespace

void IntCompressor::assignInitialAbbreviations() {
  CountNodeCollector Collector(getRoot());
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.assignInitialAbbreviations();
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
