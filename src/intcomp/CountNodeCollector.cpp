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

// Implements a collector of count nodes (i.e. abbreviation patterns), based on
// weight, and allows sorting via a heap.

#include "intcomp/CountNodeCollector.h"

namespace wasm {

using namespace filt;
using namespace utils;

namespace intcomp {

void CountNodeCollector::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClass> CountNodeCollector::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("IntCompress"));
  return Trace;
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
    collectNode(Root->getDefaultSingle(), Flags);
    collectNode(Root->getDefaultMultiple(), Flags);
  }
  for (CountNode::SuccMapIterator Iter = Root->getSuccBegin(),
                                  End = Root->getSuccEnd();
       Iter != End; ++Iter)
    collectNode(Iter->second, Flags);
}

void CountNodeCollector::collectNode(CountNode::Ptr Nd, CollectionFlags Flags) {
  bool IsIntNode = isa<IntCountNode>(*Nd);
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
    size_t Count = Nd->getCount();
    bool IsSingleton = !IsIntNode || isa<SingletonCountNode>(*Nd);
    auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
    if (hasFlag(CollectionFlag::TopLevel, Flags)) {
      CountTotal += Count;
      WeightTotal += Weight;
      if (IsIntNode) {
        if (Count < CountCutoff)
          continue;
        if (Weight < WeightCutoff)
          continue;
      }
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
    if (IsIntNode) {
      if (Count < CountCutoff)
        continue;
      if (Weight < WeightCutoff)
        continue;
    }
    if (!IsSingleton) {
      Values.push_back(CountNode::Ptr(Nd));
      CountReported += Count;
      WeightReported += Weight;
      ++NumNodesReported;
    }
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

}  // end of namespace intcomp

}  // end of namespace wasm
