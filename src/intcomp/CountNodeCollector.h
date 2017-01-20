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

// Defines a collector of count nodes (i.e. abbreviation patterns), based on
// weight, and allows sorting via a heap.

#ifndef DECOMPRESSOR_SRC_INTCOMP_COUNTNODECOLLECTOR_H
#define DECOMPRESSOR_SRC_INTCOMP_COUNTNODECOLLECTOR_H

#include "intcomp/CompressionFlags.h"
#include "intcomp/IntCountNode.h"
#include "utils/Trace.h"

namespace wasm {

namespace intcomp {

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

  explicit CountNodeCollector(CountNode::RootPtr Root);
  ~CountNodeCollector() { clear(); }

  // Note: Can only be applied when the heap is empty!
  void setCompareFcn(CountNode::CompareFcnType LtFcn);

  void collectUsingCutoffs(
      size_t CountCutoff,
      size_t WeightCutoff,
      CollectionFlags Flags = makeFlags(CollectionFlag::All));
  void collectAbbreviations();
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
  void collectNode(CountNode::Ptr Nd);

  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }
  bool hasTrace() { return bool(Trace); }
  void describe(FILE* Out);

 protected:
  std::shared_ptr<utils::TraceClass> Trace;

 private:
  // Temporaries used during collection.
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  bool CollectAbbreviations;
  CollectionFlags Flags;
  void collect();
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_COUNTNODECOLLECTOR_H
