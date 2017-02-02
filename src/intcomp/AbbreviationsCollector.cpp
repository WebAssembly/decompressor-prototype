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
    CountNode::Int2PtrMap& Assignments,
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
  {
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
      addAbbreviation(Nd, Nd->getWeight());
    }
  }
  TRACE(uint64_t, "WeightCutoff", MyFlags.WeightCutoff);
  collectUsingCutoffs(MyFlags.CountCutoff, MyFlags.WeightCutoff,
                      makeFlags(CollectionFlag::IntPaths));
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
      if (MyFlags.TrimOverriddenPatterns)
        removeAbbreviation(Nd);
      continue;
    }
    addAbbreviation(Nd);
  }
}

void AbbreviationsCollector::removeAbbreviationSuccs(CountNode::Ptr Nd) {
  if (!isa<IntCountNode>(Nd.get()))
    return;
  auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
  for (auto& Pair : *IntNd)
    removeAbbreviation(Pair.second);
}

void AbbreviationsCollector::removeAbbreviation(CountNode::Ptr Nd) {
  if (!isa<IntCountNode>(Nd.get()))
    return;
  removeAbbreviationSuccs(Nd);
  auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
  IntNd->disassociateFromHeap();
}

HuffmanEncoder::NodePtr AbbreviationsCollector::assignHuffmanAbbreviations() {
  // Start by extracting out candidates based on weight. Then use resulting
  // selected patterns as alphabet for Huffman encoding.
  assignAbbreviations();
  HuffmanRoot = Encoder->encodeSymbols();
  return HuffmanRoot;
}

void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd) {
  addAbbreviation(Nd, Nd->getWeight());
  if (MyFlags.TrimOverriddenPatterns)
    removeAbbreviationSuccs(Nd);
}

void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd,
                                             uint64_t Weight) {
  if (Nd->hasAbbrevIndex()) {
    TRACE_MESSAGE("Already has abbreviation. Ignoring");
    return;
  }
  size_t NdIndex = getNextAvailableIndex();
  TRACE(size_t, "Abbreviation", NdIndex);
  Nd->setAbbrevIndex(Encoder->createSymbol(Weight));
  Assignments[NdIndex] = Nd;
}

}  // end of namespace intcomp

}  // end of namespace wasm
