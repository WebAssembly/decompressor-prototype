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
using namespace interp;
using namespace utils;

namespace intcomp {

class CounterWriter : public Writer {
  CounterWriter() = delete;
  CounterWriter(const CounterWriter&) = delete;
  CounterWriter& operator=(const CounterWriter&) = delete;

 public:
  CounterWriter(IntCountUsageMap& UsageMap, size_t Cutoff)
      : UsageMap(UsageMap), input_seq(1), Cutoff(Cutoff) {}

  ~CounterWriter() OVERRIDE;

  void setSequenceSize(size_t Size);

  void addToUsageMap(IntType Value);

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
  IntCountUsageMap& UsageMap;
  circular_vector<IntType> input_seq;
  size_t Cutoff;
};

CounterWriter::~CounterWriter() {
}

StreamType CounterWriter::getStreamType() const {
  return StreamType::Int;
}

void CounterWriter::setSequenceSize(size_t Size) {
  input_seq.clear();
  input_seq.resize(Size);
}

void CounterWriter::addToUsageMap(IntType Value) {
  input_seq.push_back(Value);
  if (!input_seq.full())
    return;
  IntCountUsageMap* Map = &UsageMap;
  IntCountNode* Nd = nullptr;
  size_t InputSize = input_seq.size();
  for (size_t i = 0, e = InputSize; i < e; ++i) {
    IntType Val = input_seq[i];
    if (InputSize > 1 && UsageMap[Val]->getCount() < Cutoff)
      continue;
    Nd = IntCountNode::lookup(*Map, Val, Nd);
    if (i + 1 == e)
      Nd->increment();
    else
      Map = Nd->getNextUsageMap(true);
  }
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
  return true;
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                             std::shared_ptr<decode::Queue> OutputStream,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Symtab(Symtab), CountCutoff(0), WeightCutoff(0), LengthLimit(1) {
  Counter = new CounterWriter(UsageMap, WeightCutoff);
  Trace = new TraceClassSexpReader(nullptr, "IntCompress");
  Input = new Reader(InputStream, *Counter, Symtab, *Trace);
  StartPos = Input->getPos();
  (void)OutputStream;
}

IntCompressor::~IntCompressor() {
  TRACE_METHOD("~IntCompressor");
  delete Input;
  delete Counter;
  delete Trace;
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
  for (size_t i = 1; i <= LengthLimit; ++i) {
    fprintf(stderr, "Collecting integer sequences of length: %" PRIuMAX "\n",
            uintmax_t(i));
    Counter->setSequenceSize(i);
    Input->start(StartPos);
    Input->readBackFilled();
  }
  describe(stderr);
}

namespace {

struct IntSeqCollector {
  IntCountUsageMap& UsageMap;
  typedef std::vector<std::pair<size_t, IntCountNode*>> WeightedVector;
  WeightedVector Values;
  size_t WeightTotal;
  size_t CountTotal;
  size_t WeightReported;
  size_t CountReported;
  size_t NumNodesReported;
  size_t CountCutoff;
  size_t WeightCutoff;

  IntSeqCollector(IntCountUsageMap& UsageMap)
      : UsageMap(UsageMap),
        WeightTotal(0),
        CountTotal(0),
        WeightReported(0),
        CountReported(0),
        NumNodesReported(0),
        CountCutoff(1),
        WeightCutoff(1) {}
  ~IntSeqCollector() {}

  void collect();
  void collectNode(IntCountNode* Nd);
  void describe(FILE* Out);
};

void IntSeqCollector::collect() {
  for (const auto& pair : UsageMap)
    collectNode(pair.second.get());
  std::sort(Values.begin(), Values.end());
  std::reverse(Values.begin(), Values.end());
}

void IntSeqCollector::collectNode(IntCountNode* Nd) {
  size_t Weight = Nd->getWeight();
  size_t Count = Nd->getCount();
  CountTotal += Count;
  WeightTotal += Weight;
  if (Count < CountCutoff)
    return;
  if (Weight < WeightCutoff)
    return;
  Values.push_back(std::make_pair(Count, Nd));
  CountReported += Count;
  WeightReported += Weight;
  ++NumNodesReported;
  if (IntCountUsageMap* NdUsageMap = Nd->getNextUsageMap())
    for (const auto& pair : *NdUsageMap)
      collectNode(pair.second.get());
}

void IntSeqCollector::describe(FILE* Out) {
  fprintf(Out, "Total weight: %" PRIuMAX " Reported Weight %" PRIuMAX
               " Number nodes reported: %" PRIuMAX "\n",
          uintmax_t(WeightTotal), uintmax_t(WeightReported),
          uintmax_t(NumNodesReported));
  fprintf(Out, "Total count: %" PRIuMAX " Reported count %" PRIuMAX "\n",
          uintmax_t(CountTotal), uintmax_t(CountReported));
  for (const auto& pair : Values)
    pair.second->describe(Out);
}

}  // end of anonymous namespace

void IntCompressor::describe(FILE* Out) {
  IntSeqCollector Collector(UsageMap);
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.collect();
  Collector.describe(Out);
}

}  // end of namespace intcomp

}  // end of namespace wasm
