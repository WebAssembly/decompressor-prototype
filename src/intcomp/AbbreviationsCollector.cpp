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

namespace intcomp {

void AbbreviationsCollector::assignAbbreviations() {
  {
    CountNode::PtrVector Others;
    Root->getOthers(Others);
    for (CountNode::Ptr Nd : Others)
      addAbbreviation(Nd);
  }
  collect(makeFlags(CollectionFlag::All));
  buildHeap();
  while (!ValuesHeap->empty()) {
    CountNode::Ptr Nd = popHeap();
    // For now, only assume we are trying to handle integer sequences.
    if (!isa<IntSeqCountNode>(*Nd))
      continue;
    IntTypeFormats Formats(getNextAvailableIndex());
    size_t Space = Formats.getByteSize(AbbrevFormat);
    if (Space <= Nd->getWeight())
      addAbbreviation(Nd);
  }
}

}  // end of namespace intcomp

}  // end of namespace wasm
