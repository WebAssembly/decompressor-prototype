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

using namespace interp;
using namespace utils;

namespace intcomp {

AbbreviationsCollector::AbbreviationsCollector(
    CountNode::RootPtr Root,
    CountNode::PtrSet& Assignments,
    const IntCompressor::Flags& MyFlags)
    : CountNodeCollector(Root),
      Assignments(Assignments),
      MyFlags(MyFlags),
      Encoder(std::make_shared<HuffmanEncoder>()) {
}

AbbreviationsCollector::~AbbreviationsCollector() {
}

void AbbreviationsCollector::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClass> AbbreviationsCollector::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("AbbreviationCollector"));
  return Trace;
}

void AbbreviationsCollector::assignAbbreviations() {
  TRACE_METHOD("assignAbbreviations");
  TRACE(uint64_t, "WeightCutoff", MyFlags.WeightCutoff);
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
    if (isa<IntCountNode>(*Nd) && Nd->getWeight() < MyFlags.WeightCutoff) {
      TRACE_MESSAGE("Removing due to weight cutoff");
      continue;
    }
    addAbbreviation(Nd);
  }
  // Now create abbreviation indices for selected abbreviations.
  for (const CountNode::Ptr& Nd : Assignments)
    pushHeap(Nd);
  while (!ValuesHeap->empty()) {
    CountNode::Ptr Nd = popHeap();
    Nd->setAbbrevIndex(Encoder->createSymbol(Nd->getCount()));
  }
}

HuffmanEncoder::NodePtr AbbreviationsCollector::assignHuffmanAbbreviations() {
  // Start by extracting out candidates based on weight. Then use resulting
  // selected patterns as alphabet for Huffman encoding.
  assignAbbreviations();
  HuffmanRoot = Encoder->encodeSymbols();
  return HuffmanRoot;
}

void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd) {
  if (Assignments.count(Nd) > 0) {
    TRACE_MESSAGE("Ignoring: already chosen");
    return;
  }
  Assignments.insert(Nd);
  TRACE_MESSAGE("Added to assignments");
  if (!MyFlags.TrimOverriddenPatterns) {
    TRACE(size_t, "Number assignements", Assignments.size());
    return;
  }
  CountNode* NdPtr = Nd.get();
  uint64_t Count = NdPtr->getCount();
  bool ErasedParents = false;
  while (auto* IntNd = dyn_cast<IntCountNode>(NdPtr)) {
    CountNode::IntPtr Parent = IntNd->getParent();
    if (!Parent)
      break;
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
        CountNode::Ptr Nd = Parent;
        pushHeap(Nd);
      }
    }
    if (Assignments.count(Parent) > 0) {
      TRACE_MESSAGE("Removing from assignments");
      Assignments.erase(Parent);
      ErasedParents = true;
    }
    NdPtr = ParentPtr;
  }
  TRACE(size_t, "Number assignements", Assignments.size());
}

}  // end of namespace intcomp

}  // end of namespace wasm
