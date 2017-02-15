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
#include "intcomp/IntCountNode.h"
#include "interp/IntFormats.h"
#include "utils/circular-vector.h"
#include "utils/Trace.h"

#ifdef NDEBUG

#define TRACE_ABBREV_SELECTION_USING(trace, name, value)
#define TRACE_ABBREV_SELECTION(name, value)

#else

#define TRACE_ABBREV_SELECTION_USING(Trace, name, value) \
  do {                                                   \
    auto& tracE = (Trace);                               \
    if (tracE.getTraceProgress())                        \
      AbbrevSelection::trace(tracE, name, value)    ;    \
  } while(false)

#define TRACE_ABBREV_SELECTION(name, value) \
    TRACE_ABBREV_SELECTION_USING(getTrace(), name, Sel);
#endif

namespace wasm {

namespace intcomp {

class AbbrevSelection : public std::enable_shared_from_this<AbbrevSelection> {
  AbbrevSelection() = delete;
  AbbrevSelection(const AbbrevSelection&) = delete;
  AbbrevSelection& operator=(const AbbrevSelection&) = delete;
 public:
  typedef std::shared_ptr<AbbrevSelection> Ptr;
  typedef std::function<bool(Ptr, Ptr)> CompareFcnType;

  static CompareFcnType CompareGT;
  static CompareFcnType CompareLT;

  // Do not use directly! Call create to instantiate.
  AbbrevSelection(CountNode::Ptr Abbreviation,
                  Ptr Previous,
                  size_t BufferIndex,
                  size_t Weight);

  CountNode::Ptr getAbbreviation() const { return Abbreviation; }
  Ptr getPrevious() const { return Previous; }
  size_t getBufferIndex() const { return BufferIndex; }
  size_t getWeight() const;

  static Ptr create(CountNode::Ptr Abbreviation,
                    Ptr Previous,
                    size_t Weight,
                    size_t BufferIndex);

  static Ptr create(CountNode::Ptr Abbreviation,
                    size_t Weight,
                    size_t BufferIndex,
                    size_t NumLeadingDefaultValues = 0);

  // Returns < 0 if this smaller, > 0 if Sel smaller, and zero if equal.
  int compare(AbbrevSelection* Sel) const;

  void describe(FILE* Out, bool Summary=true);
  static void trace(utils::TraceClass& TC, charstring Name, AbbrevSelection::Ptr Sel);

 private:
  CountNode::Ptr Abbreviation;
  Ptr Previous;
  size_t BufferIndex;
  size_t Weight;
  // Note: CreationIndex is used to break ties in comparison.
  size_t CreationIndex;
  static size_t NextCreationIndex;
};

class AbbrevSelector {
 public:
  typedef utils::circular_vector<decode::IntType> BufferType;
  AbbrevSelector(BufferType Buffer,
                 CountNode::RootPtr Root,
                 size_t NumLeadingDefaultValues,
                 interp::IntTypeFormat AbbrevFormat,
                 const AbbrevAssignFlags& Flags);
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
  interp::IntTypeFormat AbbrevFormat;
  const AbbrevAssignFlags& Flags;
  std::shared_ptr<HeapType> Heap;
  std::map<decode::IntType, interp::IntTypeFormats*> FormatMap;
  utils::TraceClass::Ptr Trace;

  size_t computeAbbrevWeight(CountNode::Ptr Abbev);
  size_t computeValueWeight(decode::IntType Value);
  void installDefaults();
  void installDefaults(AbbrevSelection::Ptr Previous);
  void installIntSeqMatches();
  void installIntSeqMatches(AbbrevSelection::Ptr Previous);
  void installMatches();
  void installMatches(AbbrevSelection::Ptr Previous);
  AbbrevSelection::Ptr popHeap();
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVSELECTOR_H_
