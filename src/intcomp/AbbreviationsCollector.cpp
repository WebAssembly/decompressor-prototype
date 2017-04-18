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

// Implements a collector to assign abbreviations to count nodes
// (i.e. patterns).

#include "intcomp/AbbreviationsCollector.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace intcomp {

AbbreviationsCollector::AbbreviationsCollector(CountNode::RootPtr Root,
                                               CountNode::PtrSet& Assignments,
                                               const CompressionFlags& MyFlags)
    : CountNodeCollector(Root), Assignments(Assignments), MyFlags(MyFlags) {}

AbbreviationsCollector::~AbbreviationsCollector() {}

void AbbreviationsCollector::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClass> AbbreviationsCollector::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("AbbreviationCollector"));
  return Trace;
}

HuffmanEncoder::NodePtr AbbreviationsCollector::assignAbbreviations() {
  TRACE_METHOD("assignAbbreviations");
  TRACE(uint64_t, "WeightCutoff", MyFlags.WeightCutoff);
  TrimmedNodes.clear();
  {
    // Add always defined patterns.
    CountNode::PtrVector Others;
    Root->getOthers(Others);
    for (CountNode::Ptr Nd : Others) {
      TRACE_BLOCK({
        FILE* Out = getTrace().getFile();
        fprintf(Out, "Considering: ");
        Nd->describe(Out);
      });
      // Assume others are needed if even used once!
      if (Nd->getCount() == 0) {
        TRACE_MESSAGE("Ignoring: never used");
        continue;
      }
      addAbbreviation(Nd);
    }
    for (IntType Val = 0; Val < MyFlags.SmallValueMax; ++Val) {
      constexpr bool AddIfNotFound = true;
      CountNode::Ptr Nd = lookup(Root, Val, !AddIfNotFound);
      if (!Nd)
        continue;
      CountNode* NdPtr = Nd.get();
      assert(isa<SingletonCountNode>(NdPtr));
      cast<SingletonCountNode>(NdPtr)->setSmallValueKeep(true);
      TRACE_BLOCK({
        FILE* Out = getTrace().getFile();
        fprintf(Out, "Considering: ");
        NdPtr->describe(Out);
      });
      addAbbreviation(Nd);
    }
  }
  // Now select best fitting patterns, based on weight.
  collectUsingCutoffs(MyFlags.CountCutoff, MyFlags.WeightCutoff,
                      makeFlags(CollectionFlag::All));
  buildHeap();

  while (!ValuesHeap->empty() &&
         Assignments.size() < MyFlags.MaxAbbreviations) {
    CountNode::Ptr Nd = popHeap();
    TRACE_BLOCK({
      FILE* Out = getTrace().getFile();
      fprintf(Out, "Considering: ");
      Nd->describe(Out);
    });
    addAbbreviation(Nd);
  }
  TrimmedNodes.clear();
  clearHeap();
  return CountNode::assignAbbreviations(Assignments, MyFlags);
}

void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd) {
  if (Assignments.count(Nd) > 0) {
    TRACE_MESSAGE("Ignoring: already chosen");
    return;
  }
  CountNode* NdPtr = Nd.get();
  if (!NdPtr->keep(MyFlags)) {
    TRACE_MESSAGE("Removing, count/weight too small");
    return;
  }
  Assignments.insert(Nd);
  TRACE_MESSAGE("Added to assignments");
  if (!MyFlags.TrimOverriddenPatterns) {
    TRACE(size_t, "Number assignements", Assignments.size());
    return;
  }
  // Walk up prefices and trim count off from count, so that we
  // aren't counting overlapping patterns.
  if (!isa<IntCountNode>(NdPtr)) {
    TRACE(size_t, "Number assignements", Assignments.size());
    return;
  }
  uint64_t Count = NdPtr->getCount();
  CountNode::Ptr NextNd = Nd;
  while (auto* IntNd = dyn_cast<IntCountNode>(NextNd.get())) {
    CountNode::IntPtr Parent = IntNd->getParent();
    if (!Parent)
      break;
    if (TrimmedNodes.count(NextNd)) {
      TRACE_BLOCK({
        FILE* Out = getTrace().getFile();
        fprintf(Out, "Already trimmed: ");
        NextNd->describe(Out);
      });
      break;
    }
    IntCountNode* ParentPtr = Parent.get();
    size_t OldCount = ParentPtr->getCount();
    size_t NewCount = (OldCount > Count) ? (OldCount - Count) : 0;
    if (OldCount == NewCount)
      break;
    ParentPtr->setCount(NewCount);
    TRACE_BLOCK({
      FILE* Out = getTrace().getFile();
      fprintf(Out, "Updated Parent: ");
      ParentPtr->describe(Out);
    });
    if (CountNode::HeapEntryType Entry = ParentPtr->getAssociatedHeapEntry()) {
      if (!Entry->reinsert()) {
        pushHeap(Parent);
      }
    }
    if (Assignments.count(Parent) > 0 && !ParentPtr->smallValueKeep(MyFlags)) {
      TRACE_MESSAGE("Removing from assignments");
      Assignments.erase(Parent);
    }
    NextNd = Parent;
  }
  TrimmedNodes.insert(Nd);
  TRACE(size_t, "Number assignements", Assignments.size());
}

}  // end of namespace intcomp

}  // end of namespace wasm
