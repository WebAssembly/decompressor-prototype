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
    size_t CountCutoff,
    size_t WeightCutoff,
    size_t MaxAbbreviations)
      : CountNodeCollector(Root),
        MaxAbbreviations(MaxAbbreviations),
        Assignments(Assignments),
        CountCutoff(CountCutoff),
        WeightCutoff(WeightCutoff),
        Encoder(std::make_shared<HuffmanEncoder>()) {}

AbbreviationsCollector::~AbbreviationsCollector() {}

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
      addAbbreviation(Nd,
                      isa<DefaultCountNode>(Nd.get())
                      ? std::numeric_limits<uint32_t>::max()
                      : Nd->getWeight());
    }
  }
  TRACE(uint64_t, "WeightCutoff", WeightCutoff);
  collectUsingCutoffs(CountCutoff, WeightCutoff,
                      makeFlags(CollectionFlag::All));
  buildHeap();
  while (!ValuesHeap->empty() && Assignments.size() < MaxAbbreviations) {
    CountNode::Ptr Nd = popHeap();
    TRACE_BLOCK({
      FILE* Out = getTrace().getFile();
      fprintf(Out, "Considering: ");
      Nd->describe(Out);
    });
    if (isa<IntCountNode>(*Nd) && Nd->getWeight() < WeightCutoff) {
      TRACE_MESSAGE("Removing due to weight cutoff");
      continue;
    }
    addAbbreviation(Nd);
  }
  TRACE_MESSAGE("Huffman encoding abbreviations");
}

#if 0
void AbbreviationsCollector::assignHuffmanAbbreviations() {
  // Start by extracting out candidates based on weight. Then use resulting
  // selected patterns as alphabet for Huffman encoding.
  assignAbbreviations();
  CountNode::PtrVector Alphabet;
  for (const auto& Pair : Assignments) {
    Alphabet.push_back(Pair.second);
    Pair.second->clearAbbrevIndex();
  }
  Assignments.clear();
  clearHeap();
// Now build heap (sorted by smaller weights first) to assign abbreviation
// values.
#if 0
  ValuesHeap.setLtFcn(CountNode::CompareGt);
  for (auto& Nd : Alphabet)
    Nd->associateWithHeap(ValuesHeap->push(Value));
  // Create nodes over two smallest values in heap.
  while (ValuesHeap.size() > 1) {
    Ptr Nd1 = ValuesHeap.top();
    ValuesHeap.pop();
    Ptr Nd2 = ValuesHeap.top();
    ValuesHeap.pop();
  }
#endif
}
#endif


void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd) {
  addAbbreviation(Nd, Nd->getWeight());
}

void AbbreviationsCollector::addAbbreviation(CountNode::Ptr Nd, uint64_t Weight) {
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
