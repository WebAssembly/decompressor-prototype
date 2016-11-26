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

class CounterWriter : public Writer {
  CounterWriter() = delete;
  CounterWriter(const CounterWriter&) = delete;
  CounterWriter& operator=(const CounterWriter&) = delete;

 public:
  CounterWriter(IntCountUsageMap& UsageMap)
      : UsageMap(UsageMap),
        input_seq(new circular_vector<IntType>(1)),
        CountCutoff(1),
        WeightCutoff(1),
        UpToSize(0) {}

  ~CounterWriter() OVERRIDE;

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

  BlockCountNode& getBlockCount() { return BlockCount; }

// For debugging
#if DESCRIBE_INPUT
  void describeInput();
#endif

 private:
  IntCountUsageMap& UsageMap;
  BlockCountNode BlockCount;
  circular_vector<IntType>* input_seq;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  size_t UpToSize;
  void popValuesFromInputSeq(size_t Size);
};

#if DESCRIBE_INPUT
void CounterWriter::describeInput() {
  fprintf(stderr, "input seq:");
  for (size_t i = 0; i < input_seq->size(); ++i)
    fprintf(stderr, " %" PRIuMAX, uintmax_t((*input_seq).at(i)));
  fprintf(stderr, "\n");
}
#endif

CounterWriter::~CounterWriter() {
  // TODO(karlschimpf): Recover memory of input_seq. Not done now due to
  // (optimized) of destruction of circular_vector<IntType>
}

StreamType CounterWriter::getStreamType() const {
  return StreamType::Int;
}

void CounterWriter::setSequenceSize(size_t Size) {
  input_seq->clear();
  if (Size > 0)
    input_seq->resize(Size);
}

void CounterWriter::popValuesFromInputSeq(size_t Size) {
  for (size_t i = 0; i < Size; ++i) {
    if (input_seq->empty())
      return;
    input_seq->pop_front();
  }
}

void CounterWriter::addInputSeqToUsageMap() {
#if DESCRIBE_INPUT
  describeInput();
#endif
  IntCountUsageMap* Map = &UsageMap;
  IntCountNode* Nd = nullptr;
  for (size_t i = 0, e = input_seq->size(); i < e; ++i) {
    IntType Val = (*input_seq)[i];
    Nd = IntCountNode::lookup(*Map, Val, Nd);

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
    Map = &Nd->getNextUsageMap();
  }
  // If reached, no values added. Pop one value so that this
  // metod guarantees to shrink the input sequence.
  popValuesFromInputSeq(1);
}

void CounterWriter::addAllInputSeqsToUsageMap() {
  while (!input_seq->empty())
    addInputSeqToUsageMap();
}

void CounterWriter::addToUsageMap(IntType Value) {
  input_seq->push_back(Value);
  if (!input_seq->full())
    return;
  addInputSeqToUsageMap();
}

bool CounterWriter::writeUint8(uint8_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeUint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeUint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeVarint32(int32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeVarint64(int64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeVaruint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeVaruint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeValue(decode::IntType Value, const filt::Node*) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
      addAllInputSeqsToUsageMap();
      BlockCount.increment();
      return true;
    case PredefinedSymbol::Block_exit:
      addAllInputSeqsToUsageMap();
      return true;
    default:
      return true;
  }
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                             std::shared_ptr<decode::Queue> OutputStream,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Symtab(Symtab), CountCutoff(0), WeightCutoff(0), LengthLimit(1) {
  readInput(InputStream, Symtab);
  Counter = new CounterWriter(UsageMap);
  Input = new StreamReader(InputStream, *Counter, Symtab);
  (void)OutputStream;
  getTrace().setTraceProgress(true);
}

void IntCompressor::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (Trace)
    Input->setTrace(Trace);
}

TraceClassSexp& IntCompressor::getTrace() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("IntCompress"));
  return *Trace;
}

bool IntCompressor::readInput(std::shared_ptr<Queue> InputStream,
                              std::shared_ptr<SymbolTable> Symtab) {
  Contents = std::make_shared<IntStream>();
  IntWriter MyWriter(Contents);
  StreamReader MyReader(InputStream, MyWriter, Symtab);
  TraceClassSexp& Trace = MyReader.getTrace();
  Trace.addContext(MyReader.getTraceContext());
  Trace.addContext(MyWriter.getTraceContext());
  Trace.setTraceProgress(true);
  MyReader.start();
  MyReader.readBackFilled();
  return MyReader.isFinished() && MyReader.isSuccessful();
}

IntCompressor::~IntCompressor() {
  delete Input;
  delete Counter;
  IntCountNode::clear(UsageMap);
}

void IntCompressor::compressUpToSize(size_t Size) {
  fprintf(stderr,
          "Collecting integer sequences of (up to) length: %" PRIuMAX "\n",
          uintmax_t(Size));
  Counter->setUpToSize(Size);
  Input->startUsing(StartPos);
  Input->readBackFilled();
  // Flush any remaining values in input sequece.
  Counter->addAllInputSeqsToUsageMap();
  Counter->resetUpToSize();
}

void IntCompressor::removeSmallUsageCounts(IntCountUsageMap& UsageMap) {
  std::vector<IntType> KeysToRemove;
  for (const auto& pair : UsageMap) {
    if (pair.second == nullptr || removeSmallUsageCounts(pair.second))
      KeysToRemove.push_back(pair.first);
  }
  for (const auto Key : KeysToRemove)
    IntCountNode::erase(UsageMap, Key);
}

bool IntCompressor::removeSmallUsageCounts(CountNode* Nd) {
  if (Nd == nullptr)
    return true;
  {
    bool RemoveNode = false;
    if (Nd->getCount() < CountCutoff)
      RemoveNode = true;
    else if (Nd->getWeight() < WeightCutoff)
      RemoveNode = true;
    if (!RemoveNode)
      return false;
  }
  if (auto* IntNd = dyn_cast<IntCountNode>(Nd)) {
    IntCountUsageMap& NdUsageMap = IntNd->getNextUsageMap();
    removeSmallUsageCounts(NdUsageMap);
    return NdUsageMap.empty();
  }
  return false;
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
#if 0
  if (!readInput()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  Counter->setCountCutoff(CountCutoff);
  Counter->setWeightCutoff(WeightCutoff);
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // IntCountNode trie.
  compressUpToSize(1);
  removeSmallUsageCounts();
  describe(stderr, Flag(CollectionFlag::TopLevel));
  if (LengthLimit > 1) {
    compressUpToSize(LengthLimit);
    removeSmallUsageCounts();
  }
  describe(stderr, Flag(CollectionFlag::IntPaths));
#endif
}

namespace {

class CountNodeCollector {
 public:
  IntCountUsageMap& UsageMap;
  CounterWriter* Counter;
  std::vector<CountNode::HeapValueType> Values;
  std::shared_ptr<CountNode::HeapType> ValuesHeap;
  uint64_t WeightTotal;
  uint64_t CountTotal;
  uint64_t WeightReported;
  uint64_t CountReported;
  uint64_t NumNodesReported;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;

  explicit CountNodeCollector(IntCountUsageMap& UsageMap,
                              CounterWriter* Counter)
      : UsageMap(UsageMap),
        Counter(Counter),
        ValuesHeap(std::make_shared<CountNode::HeapType>()),
        WeightTotal(0),
        CountTotal(0),
        WeightReported(0),
        CountReported(0),
        NumNodesReported(0),
        CountCutoff(1),
        WeightCutoff(1) {}
  ~CountNodeCollector() { clear(); }

  void collect(CollectionFlags Flags = Flag(CollectionFlag::All));
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
  void collectNode(CountNode* Nd, CollectionFlags Flags);
  void describe(FILE* Out);
};

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
  if (hasFlag(CollectionFlag::TopLevel, Flags))
    collectNode(&Counter->getBlockCount(), Flags);
  for (const auto& pair : UsageMap)
    collectNode(pair.second, Flags);
}

void CountNodeCollector::collectNode(CountNode* Nd, CollectionFlags Flags) {
  uint64_t Weight = Nd->getWeight();
  uint64_t Count = Nd->getCount();
  auto* IntNd = dyn_cast<IntCountNode>(Nd);
  if (hasFlag(CollectionFlag::TopLevel, Flags)) {
    CountTotal += Count;
    WeightTotal += Weight;
    if (Count < CountCutoff)
      return;
    if (Weight < WeightCutoff)
      return;
    if (IntNd == nullptr || IntNd->isSingletonPath()) {
      CountReported += Count;
      WeightReported += Weight;
      ++NumNodesReported;
      Values.push_back(Nd);
    }
  }
  if (IntNd == nullptr || !hasFlag(CollectionFlag::IntPaths, Flags))
    return;
  if (!IntNd->isSingletonPath()) {
    CountTotal += Count;
    WeightTotal += Weight;
  }
  if (Count < CountCutoff)
    return;
  if (Weight < WeightCutoff)
    return;
  if (!IntNd->isSingletonPath()) {
    Values.push_back(CountNode::Ptr(Nd));
    CountReported += Count;
    WeightReported += Weight;
    ++NumNodesReported;
  }
  IntCountUsageMap& NdUsageMap = IntNd->getNextUsageMap();
  for (const auto& pair : NdUsageMap)
    collectNode(pair.second, Flags);
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

void IntCompressor::describe(FILE* Out, CollectionFlags Flags) {
  fprintf(stderr, "Collecting results...\n");
  CountNodeCollector Collector(UsageMap, Counter);
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.collect(Flags);
  Collector.describe(Out);
}

}  // end of namespace intcomp

}  // end of namespace wasm
