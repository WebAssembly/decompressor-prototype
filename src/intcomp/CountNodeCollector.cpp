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

CountNodeCollector::CountNodeCollector(CountNode::RootPtr Root)
    : Root(Root),
      ValuesHeap(std::make_shared<CountNode::HeapType>(CountNode::CompareLt)),
      WeightTotal(0),
      CountTotal(0),
      WeightReported(0),
      CountReported(0),
      NumNodesReported(0),
      CountCutoff(0),
      WeightCutoff(0),
      CollectAbbreviations(false),
      Flags(makeFlags(CollectionFlag::None)) {
}

void CountNodeCollector::setCompareFcn(CountNode::CompareFcnType LtFcn) {
  assert(ValuesHeap->empty());
  ValuesHeap->setLtFcn(LtFcn);
}

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
    pushHeap(Value);
}

void CountNodeCollector::pushHeap(CountNode::Ptr Nd) {
  assert(ValuesHeap);
  Nd->associateWithHeap(ValuesHeap->push(Nd));
}

CountNode::HeapValueType CountNodeCollector::popHeap() {
  assert(ValuesHeap);
  CountNode::HeapEntryType Entry = ValuesHeap->top();
  ValuesHeap->pop();
  return Entry->getValue();
}

void CountNodeCollector::describeHeap(FILE* Out) {
  ValuesHeap->describe(Out, [](FILE* Out, CountNode::HeapValueType Value) {
    Value->describe(Out);
  });
}

void CountNodeCollector::collectUsingCutoffs(size_t MyCountCutoff,
                                             size_t MyWeightCutoff,
                                             CollectionFlags MyFlags) {
  TRACE_METHOD("collectUsingCutoffs");
  CountCutoff = MyCountCutoff;
  WeightCutoff = MyWeightCutoff;
  CollectAbbreviations = false;
  Flags = MyFlags;
  TRACE(uint32_t, "Flags", Flags);
  TRACE(uint64_t, "CountCutoff", CountCutoff);
  TRACE(uint64_t, "WeightCutoff", WeightCutoff);
  collect();
}

void CountNodeCollector::collectAbbreviations() {
  TRACE_METHOD("collectAbbreviations");
  CountCutoff = 0;
  WeightCutoff = 0;
  Flags = makeFlags(CollectionFlag::All);
  CollectAbbreviations = true;
  collect();
}

void CountNodeCollector::collect() {
  if (hasFlag(CollectionFlag::TopLevel, Flags)) {
    CountNode::PtrVector Others;
    Root->getOthers(Others);
    for (CountNode::Ptr Nd : Others)
      collectNode(Nd);
  }
  for (auto& Pair : *Root.get())
    collectNode(Pair.second);
}

void CountNodeCollector::collectNode(CountNode::Ptr Nd) {
  TRACE_METHOD("collectNode");
  std::vector<CountNode::Ptr> ToAdd;
  ToAdd.push_back(Nd);
  while (!ToAdd.empty()) {
    Nd = ToAdd.back();
    ToAdd.pop_back();
    if (!Nd)  // This shouldn't happen, but be safe.
      continue;
    TRACE_BLOCK({ Nd->describe(getTrace().getFile()); });
    auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
    bool IsIntNode = IntNd != nullptr;
    uint64_t Weight = Nd->getWeight();
    size_t Count = Nd->getCount();
    bool IsSingleton = !IsIntNode || isa<SingletonCountNode>(*Nd);
    if (IsIntNode)
      for (auto& Pair : *IntNd)
        ToAdd.push_back(Pair.second);
    if (hasFlag(CollectionFlag::TopLevel, Flags)) {
      CountTotal += Count;
      WeightTotal += Weight;
      if (CollectAbbreviations) {
        if (!Nd->hasAbbrevIndex())
          continue;
      } else {
        if (IsIntNode) {
          if (IsSingleton && Count < CountCutoff) {
            TRACE_MESSAGE("Omitting due to count cutoff");
            continue;
          }
          if (Weight < WeightCutoff) {
            TRACE_MESSAGE("Omitting due to weight cutoff");
            continue;
          }
        }
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
      if (CollectAbbreviations) {
        if (!Nd->hasAbbrevIndex())
          continue;
      } else {
        if (IsSingleton && Count < CountCutoff) {
          TRACE_MESSAGE("Omitting due to count cutoff");
          continue;
        }
        if (Weight < WeightCutoff) {
          TRACE_MESSAGE("Omitting due to weight cutoff");
          continue;
        }
      }
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
