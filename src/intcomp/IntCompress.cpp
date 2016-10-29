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
#include "utils/circular-vector.h"
#include <vector>
#include <algorithm>

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
      : UsageMap(UsageMap), input_seq(new circular_vector<IntType>(1)), CountCutoff(1),
        WeightCutoff(1), UpToSize(0) {}

  ~CounterWriter() OVERRIDE;

  void setSequenceSize(size_t Size);
  void setCountCutoff(uint64_t NewValue) { CountCutoff = NewValue; }
  void setWeightCutoff(uint64_t NewValue) { WeightCutoff = NewValue; }
  void setUpToSize(size_t NewSize) { UpToSize = NewSize; }
  void resetUpToSize() { UpToSize = 0; }
  size_t geUpToSize() const { return UpToSize; }

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

  // For debugging
#if 0
  void describeInput();
#endif

 private:
  IntCountUsageMap& UsageMap;
  circular_vector<IntType>* input_seq;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  size_t UpToSize;
  void popValuesFromInputSeq(size_t Size);
};

#if 0
void CounterWriter::describeInput() {
  fprintf(stderr, "input seq:");
  for (size_t i = 0; i < input_seq->size(); ++i)
    fprintf(stderr, " %" PRIuMAX , uintmax_t((*input_seq).at(i)));
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
  IntCountUsageMap* Map = &UsageMap;
  IntCountNode* Nd = nullptr;
  for (size_t i = 0, e = input_seq->size(); i < e; ++i) {
    IntType Val = (*input_seq)[i];
    Nd = IntCountNode::lookup(*Map, Val, Nd);

    if (UpToSize == 1 || i > 0)
      Nd->increment();
    if (e > 1) {
      // TODO(karlschimpf) We probably want to make this cutoff aware of
      // the path length, so that long sequences are more likely to be
      // found.
      if (Nd->getCount() < CountCutoff ||
          Nd->getWeight() < WeightCutoff) {
        popValuesFromInputSeq(i == 0 ? 1 : i);
        return;
      }
    }
    if (i + 1 == e) {
      popValuesFromInputSeq(e);
      return;
    }
    Map = Nd->getNextUsageMap();
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
    case PredefinedSymbol::BlockEnter:
      addAllInputSeqsToUsageMap();
      return true;
    case PredefinedSymbol::BlockExit:
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
  Counter = new CounterWriter(UsageMap);
  Trace = new TraceClassSexpReader(nullptr, "IntCompress");
  Input = new Reader(InputStream, *Counter, Symtab, *Trace);
  StartPos = Input->getPos();
  (void)OutputStream;
}

IntCompressor::~IntCompressor() {
  delete Input;
  delete Counter;
  delete Trace;
  IntCountNode::destroy(UsageMap);
}

void IntCompressor::compressUpToSize(size_t Size) {
  fprintf(stderr, "Collecting integer sequences of (up to) length: %" PRIuMAX "\n",
          uintmax_t(Size));
  Counter->setUpToSize(Size);
  Counter->setSequenceSize(Size);
  Input->start(StartPos);
  Input->readBackFilled();
  // Flush any remaining values in input sequece.
  Counter->addAllInputSeqsToUsageMap();
  Counter->resetUpToSize();
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
  Counter->setCountCutoff(CountCutoff);
  Counter->setWeightCutoff(WeightCutoff);
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // IntCountNode trie.
  compressUpToSize(1);
  describe(stderr, 1);
  if (LengthLimit > 1)
    compressUpToSize(LengthLimit);
  describe(stderr, 2);
}

namespace {

struct IntSeqCollector {
  IntCountUsageMap& UsageMap;
  typedef std::vector<std::pair<uint64_t, IntCountNode*>> WeightedVector;
  WeightedVector Values;
  uint64_t WeightTotal;
  uint64_t CountTotal;
  uint64_t WeightReported;
  uint64_t CountReported;
  uint64_t NumNodesReported;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  size_t MinPathLength;

  IntSeqCollector(IntCountUsageMap& UsageMap)
      : UsageMap(UsageMap),
        WeightTotal(0),
        CountTotal(0),
        WeightReported(0),
        CountReported(0),
        NumNodesReported(0),
        CountCutoff(1),
        WeightCutoff(1),
        MinPathLength(1) {}
  ~IntSeqCollector() {}

  void collect();
  void collectNode(IntCountNode* Nd);
  void describe(FILE* Out);
};

void IntSeqCollector::collect() {
  for (const auto& pair : UsageMap)
    collectNode(pair.second);
  std::sort(Values.begin(), Values.end());
  std::reverse(Values.begin(), Values.end());
}

void IntSeqCollector::collectNode(IntCountNode* Nd) {
  uint64_t Weight = Nd->getWeight();
  uint64_t Count = Nd->getCount();
  CountTotal += Count;
  WeightTotal += Weight;
  if (Count < CountCutoff)
    return;
  if (Weight < WeightCutoff)
    return;
  if (Nd->pathLength() >= MinPathLength)
    Values.push_back(std::make_pair(Weight, Nd));
  CountReported += Count;
  WeightReported += Weight;
  ++NumNodesReported;
  if (IntCountUsageMap* NdUsageMap = Nd->getNextUsageMap())
    for (const auto& pair : *NdUsageMap)
      collectNode(pair.second);
}

void IntSeqCollector::describe(FILE* Out) {
  fprintf(Out, "Total weight: %" PRIuMAX " Reported Weight %" PRIuMAX
               " Number nodes reported: %" PRIuMAX "\n",
          uintmax_t(WeightTotal), uintmax_t(WeightReported),
          uintmax_t(NumNodesReported));
  fprintf(Out, "Total count: %" PRIuMAX " Reported count %" PRIuMAX "\n",
          uintmax_t(CountTotal), uintmax_t(CountReported));
  size_t Count = 0;
  for (const auto& pair : Values) {
    ++Count;
    fprintf(Out, "[%" PRIuMAX "] ", uintmax_t(Count));
    pair.second->describe(Out);
  }
}

}  // end of anonymous namespace

void IntCompressor::describe(FILE* Out, size_t MinPathLength) {
  fprintf(stderr, "Collecting results...\n");
  IntSeqCollector Collector(UsageMap);
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.MinPathLength = MinPathLength;
  Collector.collect();
  Collector.describe(Out);
}

}  // end of namespace intcomp

}  // end of namespace wasm
