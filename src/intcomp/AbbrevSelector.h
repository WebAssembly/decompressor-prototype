/* -*- C++ -*- */
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

// Defines an API for deciding abbreviation layout for a buffer.

#ifndef DECOMPRESSOR_SRC_INTCOMP_ABBREVSELECTOR_H_
#define DECOMPRESSOR_SRC_INTCOMP_ABBREVSELECTOR_H_

#include "intcomp/CompressionFlags.h"
#include "intcomp/CountNode.h"
#include "interp/IntFormats.h"
#include "utils/Trace.h"
#include "utils/circular-vector.h"

#ifdef NDEBUG

#define TRACE_ABBREV_SELECTION_USING(trace, name, value)
#define TRACE_ABBREV_SELECTION(name, value)

#else

#define TRACE_ABBREV_SELECTION_USING(Trace, name, value) \
  do {                                                   \
    auto& tracE = (Trace);                               \
    if (tracE.getTraceProgress())                        \
      AbbrevSelection::trace(tracE, name, value);        \
  } while (false)

#define TRACE_ABBREV_SELECTION(name, value) \
  TRACE_ABBREV_SELECTION_USING(getTrace(), name, value);
#endif

namespace wasm {

namespace intcomp {

class AbbrevSelector;

class AbbrevSelection : public std::enable_shared_from_this<AbbrevSelection> {
  AbbrevSelection() = delete;
  AbbrevSelection(const AbbrevSelection&) = delete;
  AbbrevSelection& operator=(const AbbrevSelection&) = delete;
  friend class AbbrevSelector;

 public:
  typedef std::shared_ptr<AbbrevSelection> Ptr;

  // Do not use directly! Call AbbrevSelector::create to instantiate.
  AbbrevSelection(CountNode::Ptr Abbreviation,
                  Ptr Previous,
                  size_t IntsConsumed,
                  size_t Weight,
                  size_t CreationIndex);

  CountNode::Ptr getAbbreviation() const { return Abbreviation; }
  Ptr getPrevious() const { return Previous; }
  size_t getIntsConsumed() const { return IntsConsumed; }
  size_t getWeight() const { return Weight; }
  size_t getCreationIndex() const { return CreationIndex; }

  void describe(FILE* Out, bool Summary = true);
  static void trace(utils::TraceClass& TC,
                    charstring Name,
                    AbbrevSelection::Ptr Sel);

 private:
  CountNode::Ptr Abbreviation;
  Ptr Previous;
  size_t IntsConsumed;
  size_t Weight;
  // Note: Code assumes that this value must be unique for each instance,
  // and it is the responsability of the instance creator to guarantee this.
  size_t CreationIndex;
};

class AbbrevSelector {
 public:
  typedef utils::circular_vector<decode::IntType> BufferType;
  AbbrevSelector(BufferType Buffer,
                 CountNode::RootPtr Root,
                 size_t NumLeadingDefaultValues,
                 const CompressionFlags& Flags);
  // Heuristically finds the best (measured by weight) abberviation selection
  // for the contents of the buffer.
  AbbrevSelection::Ptr select();

  void setTrace(utils::TraceClass::Ptr Trace);
  utils::TraceClass::Ptr getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }
  bool hasTrace() { return bool(Trace); }

 private:
  typedef utils::heap<AbbrevSelection::Ptr> HeapType;
  BufferType Buffer;
  CountNode::RootPtr Root;
  size_t NumLeadingDefaultValues;
  size_t NextCreationIndex;
  const CompressionFlags& Flags;
  std::shared_ptr<HeapType> Heap;
  std::map<decode::IntType, interp::IntTypeFormats*> FormatMap;
  utils::TraceClass::Ptr Trace;

  AbbrevSelection::Ptr create(CountNode::Ptr Abbreviation,
                              AbbrevSelection::Ptr Previous,
                              size_t LocalWeight,
                              size_t LocalIntsConsumed);

  size_t computeAbbrevWeight(CountNode::Ptr Abbev);
  size_t computeValueWeight(decode::IntType Value);
  void createDefaults(AbbrevSelection::Ptr Previous);
  void createIntSeqMatches(AbbrevSelection::Ptr Previous);
  void createMatches(AbbrevSelection::Ptr Previous);
  void createMatches();
  AbbrevSelection::Ptr popHeap();
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVSELECTOR_H_
