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

// Defines a collector to assign abbreviations to count nodes (i.e. patterns).

#ifndef DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONSCOLLECTOR_H
#define DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONSCOLLECTOR_H

#include "intcomp/CountNodeCollector.h"

namespace wasm {

namespace intcomp {

class AbbreviationsCollector : public CountNodeCollector {
 public:
  AbbreviationsCollector(CountNode::RootPtr Root,
                         interp::IntTypeFormat AbbrevFormat,
                         CountNode::Int2PtrMap& Assignments,
                         size_t CountCutoff,
                         size_t WeightCutoff,
                         size_t MaxAbbreviations)
      : CountNodeCollector(Root),
        MaxAbbreviations(MaxAbbreviations),
        AbbrevFormat(AbbrevFormat),
        Assignments(Assignments),
        CountCutoff(CountCutoff),
        WeightCutoff(WeightCutoff) {}

  // Does assignment based on maximizing weight. This will find the set of
  // candidate patterns to use as abbreviations.
  void assignAbbreviations();

  // Does assignment based on Huffman encoding.
  void assignHuffmanAbbreviations();

  utils::TraceClass& getTrace() { return *getTracePtr(); }
  std::shared_ptr<utils::TraceClass> getTracePtr();
  void setTrace(std::shared_ptr<utils::TraceClass> NewTrace);
  bool hasTrace() const { return bool(Trace); }

 private:
  const size_t MaxAbbreviations;
  interp::IntTypeFormat AbbrevFormat;
  CountNode::Int2PtrMap& Assignments;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  std::shared_ptr<utils::TraceClass> Trace;

  size_t getNextAvailableIndex() const { return Assignments.size(); }

  void addAbbreviation(CountNode::Ptr Nd);
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONSCOLLECTOR_H
