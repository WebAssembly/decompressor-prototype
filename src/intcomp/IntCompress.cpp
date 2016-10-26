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
  CounterWriter(IntCountUsageMap& UsageMap)
      : UsageMap(UsageMap), input_seq(1) {}

  ~CounterWriter() OVERRIDE;

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
};

CounterWriter::~CounterWriter() {}

StreamType CounterWriter::getStreamType() const { return StreamType::Int; }

void CounterWriter::addToUsageMap(IntType Value) {
  input_seq.push_back(Value);
  if (!input_seq.full())
    return;
  IntCountUsageMap* Map = &UsageMap;
  for (size_t i = 0, e = input_seq.size(); i < e; ++i) {
    IntType Val = input_seq[i];
    IntCountNode* Nd = IntCountNode::add(*Map, Val);
    if (i + 1 < e)
      Map = Nd->getNextUsageMap();
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

bool CounterWriter::writeValue(decode::IntType Value,
                               const filt::Node*) {
  addToUsageMap(Value);
  return true;
}

bool CounterWriter::writeAction(const filt::CallbackNode* Action) {
  return true;
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                             std::shared_ptr<decode::Queue> OutputStream,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Symtab(Symtab)
{
  Output = new CounterWriter(UsageMap);
  ReadCursor ReadPos;
  Trace = new TraceClassSexpReader(&ReadPos, "IntCompress");
  Input = new Reader(InputStream, *Output, Symtab, *Trace);
  Trace->setReadPos(&Input->getPos());
  (void)OutputStream;
}

IntCompressor::~IntCompressor() {
  delete Input;
  delete Output;
  delete Trace;
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
  Input->start();
  Input->readBackFilled();
  describe(stderr);
}

void IntCompressor::describe(FILE* Out) {
  std::vector<std::pair<size_t , IntCountNode*>> Values;
  size_t Total = 0;
  // Sort by number of occurrences for paths.
  for (const auto& pair : UsageMap) {
    if (pair.second->getCount() < 100)
      continue;
    Total += pair.second->getCount();
    Values.push_back(std::make_pair(pair.second->pathLength()
                                    * pair.second->getCount(),
                                    pair.second.get()));
  }
  std::sort(Values.begin(), Values.end());
  std::reverse(Values.begin(), Values.end());
  fprintf(Out, "Total count: %" PRIuMAX "\n", uintmax_t(Total));
  for (const auto& pair : Values)
    pair.second->describe(Out);
}

}  // end of namespace intcomp

}  // end of namespace wasm
