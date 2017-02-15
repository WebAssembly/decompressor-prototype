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

// Implements an API for deciding abbreviation layout for a buffer.

#include "intcomp/AbbrevSelector.h"

#include <cassert>

#ifdef NODEBUG

#define IF_TRACE(level, tracecall)

#else

#define IF_TRACE(Flag, tracecall)         \
  if (Flags.TraceAbbrevSelection##Flag) { \
    tracecall;                            \
  }

#endif

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace intcomp {

namespace {

size_t countIntPatterns(AbbrevSelection* Sel) {
  size_t Count = 0;
  while (Sel != nullptr) {
    if (isa<IntCountNode>(Sel->getAbbreviation().get()))
      ++Count;
    Sel = Sel->getPrevious().get();
  }
  return Count;
}

bool isHillclimbLT(AbbrevSelection::Ptr S1, AbbrevSelection::Ptr S2) {
  AbbrevSelection* Sel1 = S1.get();
  AbbrevSelection* Sel2 = S2.get();
  if (Sel1 == nullptr)
    return Sel2 != nullptr;
  if (Sel2 == nullptr)
    return false;
  // To force a quick base to compare against during hillclimbing, favor
  // selections that consume more input first. This should also favor selections
  // that use integer sequence patterns (which is good).  In other words (based
  // on consuming input) force the heap to do a depth-first search.
  size_t Count1 = Sel1->getIntsConsumed();
  size_t Count2 = Sel2->getIntsConsumed();
  if (Count1 < Count2)
    return false;
  if (Count1 > Count2)
    return true;
  // Favor the one which requires the least amount of (approximated) space.
  Count1 = Sel1->getWeight();
  Count2 = Sel2->getWeight();
  if (Count1 < Count2)
    return true;
  if (Count1 > Count2)
    return false;
  // Arbitrate by choosing the one with the most integer patterns, assuming that
  // the Huffman encoding will generate a smaller value.
  Count1 = countIntPatterns(Sel1);
  Count2 = countIntPatterns(Sel2);
  if (Count1 < Count2)
    return false;
  if (Count1 > Count2)
    return true;
  // Arbitrate uncomparable by choosing the one created first.
  Count1 = Sel1->getCreationIndex();
  Count2 = Sel2->getCreationIndex();
  if (Count1 < Count2)
    return true;
  if (Count1 > Count2)
    return false;
  WASM_RETURN_UNREACHABLE(false);
}

}  // end of anonymous namespace

AbbrevSelection::AbbrevSelection(CountNode::Ptr Abbreviation,
                                 Ptr Previous,
                                 size_t IntsConsumed,
                                 size_t Weight,
                                 size_t CreationIndex)
    : Abbreviation(Abbreviation),
      Previous(Previous),
      IntsConsumed(IntsConsumed),
      Weight(Weight),
      CreationIndex(CreationIndex) {
}

void AbbrevSelection::trace(TraceClass& TC,
                            charstring Name,
                            AbbrevSelection::Ptr Sel) {
  TC.indent();
  FILE* Out = TC.getFile();
  TC.trace_value_label(Name);
  fputc('\n', Out);
  std::vector<AbbrevSelection*> Stack;
  AbbrevSelection* Next = Sel.get();
  while (Next) {
    Stack.push_back(Next);
    Next = Next->Previous.get();
  }
  while (!Stack.empty()) {
    Stack.back()->describe(TC.indentNewline());
    Stack.pop_back();
  }
}

void AbbrevSelection::describe(FILE* Out, bool Summary) {
  AbbrevSelection* Next = this;
  do {
    fprintf(Out, "[%" PRIuMAX "] ints=%" PRIuMAX " w=%" PRIuMAX ":",
            uintmax_t(Next->CreationIndex), uintmax_t(Next->IntsConsumed),
            uintmax_t(Next->Weight));
    if (Next->Abbreviation)
      Next->Abbreviation->describe(Out);
    else
      fprintf(Out, "nullptr\n");
    Next = Next->Previous.get();
  } while (Next && !Summary);
}

AbbrevSelector::AbbrevSelector(BufferType Buffer,
                               CountNode::RootPtr Root,
                               size_t NumLeadingDefaultValues,
                               IntTypeFormat AbbrevFormat,
                               const AbbrevAssignFlags& Flags)
    : Buffer(Buffer),
      Root(Root),
      NumLeadingDefaultValues(NumLeadingDefaultValues),
      AbbrevFormat(AbbrevFormat),
      NextCreationIndex(0),
      Flags(Flags),
      Heap(std::make_shared<HeapType>(isHillclimbLT)) {
}

void AbbrevSelector::setTrace(TraceClass::Ptr NewTrace) {
  Trace = NewTrace;
}

utils::TraceClass::Ptr AbbrevSelector::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("AbbrevSelector"));
  return Trace;
}

size_t AbbrevSelector::computeAbbrevWeight(CountNode::Ptr) {
  // For now, assume all abbreviations use one byte.
  return 1;
}

size_t AbbrevSelector::computeValueWeight(IntType Value) {
  IntTypeFormats Formatter(Value);
  return Formatter.getByteSize(AbbrevFormat);
}

AbbrevSelection::Ptr AbbrevSelector::create(CountNode::Ptr Abbreviation,
                                            AbbrevSelection::Ptr Previous,
                                            size_t LocalWeight,
                                            size_t LocalIntsConsumed) {
  if (Previous) {
    AbbrevSelection* PreviousPtr = Previous.get();
    LocalWeight += PreviousPtr->getWeight();
    LocalIntsConsumed += PreviousPtr->getIntsConsumed();
  }
  return std::make_shared<AbbrevSelection>(Abbreviation, Previous,
                                           LocalIntsConsumed, LocalWeight,
                                           NextCreationIndex++);
}

void AbbrevSelector::createDefaults(AbbrevSelection::Ptr Previous) {
  IF_TRACE(Detail, TRACE_MESSAGE("Try default match"));
  AbbrevSelection::Ptr Sel;
  bool HasPrevious = bool(Previous);
  size_t Index = HasPrevious ? Previous->getIntsConsumed() : 0;
  size_t ValueWeight = computeValueWeight(Buffer[Index]);
  bool AddMultCounterSize = false;
  bool IsSingle;
  if (HasPrevious) {
    if (auto* DefNd =
            dyn_cast<DefaultCountNode>(Previous->getAbbreviation().get())) {
      IsSingle = false;
      if (DefNd->isSingle()) {
        AddMultCounterSize = true;
      }
    } else {
      IsSingle = true;
    }
  } else {
    IsSingle = (NumLeadingDefaultValues = 0);
    if (NumLeadingDefaultValues == 1) {
      AddMultCounterSize = true;
    }
  }
  IF_TRACE(Detail, {
    TRACE(bool, "Is single", IsSingle);
    TRACE(size_t, "Value weight", ValueWeight);
    TRACE(bool, "Add Multiple byte counter", AddMultCounterSize);
  });
  if (AddMultCounterSize)
    ++ValueWeight;
  if (IsSingle) {
    CountNode::DefaultPtr Default = Root->getDefaultSingle();
    Sel = create(Default, Previous, ValueWeight + computeAbbrevWeight(Default),
                 1);
  } else {
    CountNode::DefaultPtr Default = Root->getDefaultMultiple();
    Sel = create(Root->getDefaultMultiple(), Previous, ValueWeight, 1);
  }
  IF_TRACE(Create, TRACE_ABBREV_SELECTION("create", Sel));
  Heap->push(Sel);
}

void AbbrevSelector::createIntSeqMatches(AbbrevSelection::Ptr Previous) {
  IF_TRACE(Detail, TRACE_MESSAGE("Try int sequence match"));
  CountNode::IntPtr Nd;
  constexpr bool AddIfNotFound = true;
  bool HasPrevious = bool(Previous);
  size_t StartIndex = HasPrevious ? Previous->getIntsConsumed() : 0;
  for (size_t i = StartIndex; i < Buffer.size(); ++i) {
    IntType Value = Buffer[i];
    IF_TRACE(Detail, {
      TRACE(size_t, "i", i);
      TRACE(IntType, "Value", Value);
    });
    Nd = Nd ? lookup(Nd, Value, !AddIfNotFound)
            : lookup(Root, Value, !AddIfNotFound);
    if (!Nd) {
      IF_TRACE(Detail, TRACE_MESSAGE("No more patterns found!"));
      return;
    }
    if (!Nd->hasAbbrevIndex())
      continue;
    auto Sel =
        create(Nd, Previous, computeAbbrevWeight(Nd), Nd->getPathLength());
    IF_TRACE(Create, TRACE_ABBREV_SELECTION("create", Sel));
    Heap->push(Sel);
  }
}

void AbbrevSelector::createMatches(AbbrevSelection::Ptr Previous) {
  // Note: Try integer (specialized) pattern matches second, under the
  // assumption that we favor default patterns. This ordering is used
  // to arbitrate unorderable selections that will use creation index
  // as final arbitration.
  createDefaults(Previous);
  createIntSeqMatches(Previous);
}

void AbbrevSelector::createMatches() {
  AbbrevSelection::Ptr Empty;
  createMatches(Empty);
}

AbbrevSelection::Ptr AbbrevSelector::popHeap() {
  assert(Heap);
  HeapType::entry_ptr Entry = Heap->top();
  Heap->pop();
  return Entry->getValue();
}

AbbrevSelection::Ptr AbbrevSelector::select() {
  TRACE_METHOD("select");
  (void)Flags;
  AbbrevSelection::Ptr Min;
  if (Buffer.size() == 0)
    return Min;
  // Create possible defaults.
  Heap->clear();
  createMatches();
  while (!Heap->empty()) {
    // Get candidate selection.
    IF_TRACE(Detail, {
      TRACE(size_t, "heap size", Heap->size());
      TRACE(size_t, "Buffer size", Buffer.size());
    });
    AbbrevSelection::Ptr Sel = popHeap();
    IF_TRACE(Select, TRACE_ABBREV_SELECTION("Select", Sel));
    assert(bool(Sel));

    // Rule out special cases where we can short-circuit the search.
    if (!Min) {
      if (Heap->empty()) {
        // If only one choice left, no need to expand further, it must be
        // the best choice.
        IF_TRACE(Select,
                 TRACE_MESSAGE("Defining as minimum, since only selection"));
        Min = Sel;
        break;
      }
    } else if (Sel->getWeight() > Min->getWeight()) {
      IF_TRACE(Select, TRACE_MESSAGE("Ignoring: can't be minimum"));
      continue;
    }

    if (Sel->getIntsConsumed() >= Buffer.size()) {
      // Valid selection, see if best.
      if (!Min || isHillclimbLT(Sel, Min)) {
        Min = Sel;
        IF_TRACE(Select, TRACE_MESSAGE("Define as minimum"));
      } else {
        IF_TRACE(Select, TRACE_MESSAGE("Ignoring: not minimum"));
      }
      continue;
    }

    // Can't conclude best found, try more matches.
    createMatches(Sel);
  }
  TRACE_ABBREV_SELECTION("Selected min", Min);
  return Min;
}

}  // end of namespace intcomp

}  // end of namespace wasm
