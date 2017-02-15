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

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace intcomp {

size_t AbbrevSelection::NextCreationIndex = 0;

AbbrevSelection::AbbrevSelection(CountNode::Ptr Abbreviation,
                                 Ptr Previous,
                                 size_t BufferIndex,
                                 size_t Weight)
    : Abbreviation(Abbreviation),
      Previous(Previous),
      BufferIndex(BufferIndex),
      Weight(Weight),
      CreationIndex(NextCreationIndex++) {}

AbbrevSelection::Ptr AbbrevSelection::create(
    CountNode::Ptr Abbreviation,
    Ptr Previous,
    size_t Weight,
    size_t BufferIndex) {
  auto Sel = std::make_shared<AbbrevSelection>(Abbreviation, Previous, BufferIndex, Weight);
  return Sel;
}

AbbrevSelection::Ptr AbbrevSelection::create(
    CountNode::Ptr Abbreviation,
    size_t Weight,
    size_t BufferIndex,
    size_t NumLeadingDefaultValues) {
  Ptr Previous;
  auto Sel = std::make_shared<AbbrevSelection>(Abbreviation, Previous, BufferIndex, Weight);
  // TODO(karlschimpf) Set additional fields.
  return Sel;
}

int AbbrevSelection::compare(AbbrevSelection* Sel) const {
  if (Sel == nullptr)
    return 1;
  // Assume the one with the less weight (i.e. bitsize when written), is the
  // better choice.
  if (Weight < Sel->Weight)
    return -1;
  if (Weight > Sel->Weight)
    return 1;
  // Consider the one that has consumed more of the input to be the better
  // choice.
  if (BufferIndex > Sel->BufferIndex)
    return -1;
  if (BufferIndex < Sel->BufferIndex)
    return 1;
  // Both are equally likely.
  if (CreationIndex < Sel->CreationIndex)
    return -1;
  if (CreationIndex > Sel->CreationIndex)
    return 1;
  return 0;
}

namespace {

bool isLT(AbbrevSelection::Ptr S1, AbbrevSelection::Ptr S2) {
  if (!S1)
    return bool(S2);
  if (!S2)
    return false;
  return S1->compare(S2.get()) < 0;
}

bool isGT(AbbrevSelection::Ptr S1, AbbrevSelection::Ptr S2) {
  if (!S1)
    return false;
  if (!S2)
    return true;
  return S1->compare(S2.get()) > 0;
}

} // end of anonymous namespace

AbbrevSelection::CompareFcnType AbbrevSelection::CompareGT =
    [](Ptr S1, Ptr S2) { return isGT(S1, S2); };

AbbrevSelection::CompareFcnType AbbrevSelection::CompareLT =
    [](Ptr S1, Ptr S2) { return isLT(S1, S2); };

void AbbrevSelection::trace(TraceClass& TC, charstring Name, AbbrevSelection::Ptr Sel) {
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
            uintmax_t(Next->CreationIndex), uintmax_t(Next->BufferIndex),
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
      Flags(Flags),
      Heap(std::make_shared<HeapType>(AbbrevSelection::CompareLT)) {
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

void AbbrevSelector::installDefaults() {
  TRACE_METHOD("installlDefaults");
  AbbrevSelection::Ptr Sel;
  size_t ValueWeight = computeValueWeight(Buffer[0]);
  if (NumLeadingDefaultValues == 0) {
    CountNode::DefaultPtr Default = Root->getDefaultSingle();
    Sel = AbbrevSelection::create(Default,
                                  ValueWeight + computeAbbrevWeight(Default),
                                  1);
  } else {
    CountNode::DefaultPtr Default = Root->getDefaultMultiple();
    Sel = AbbrevSelection::create(Root->getDefaultMultiple(),
                                  ValueWeight,
                                  1);
  }
  TRACE_ABBREV_SELECTION("install", Sel);
  Heap->push(Sel);
}

void AbbrevSelector::installIntSeqMatches() {
  TRACE_METHOD("installMatches");
  CountNode::IntPtr Nd;
  constexpr bool AddIfNotFound = true;
  for (size_t i = 0; i < Buffer.size(); ++i) {
    IntType Value = Buffer[i];
    TRACE(size_t, "i", i);
    TRACE(IntType, "Value", Value);
    Nd = Nd ? lookup(Nd, Value, !AddIfNotFound)
            : lookup(Root, Value, !AddIfNotFound);
    if (!Nd) {
      TRACE_MESSAGE("No more patterns found!");
      return;
    }
    if (!Nd->hasAbbrevIndex())
      continue;
    auto Sel = AbbrevSelection::create(Nd,
                                       computeAbbrevWeight(Nd),
                                       i + 1);
    TRACE_ABBREV_SELECTION("install", Sel);
    Heap->push(Sel);
  }
}

void AbbrevSelector::installMatches() {
  installDefaults();
  installIntSeqMatches();
}

void AbbrevSelector::installDefaults(AbbrevSelection::Ptr Previous) {
  // TODO(karlschimpf): Define.
#if 1
  (void) Previous;
#endif
}

void AbbrevSelector::installIntSeqMatches(AbbrevSelection::Ptr Previous) {
  // TODO(karlschimpf): Define.
#if 1
  (void) Previous;
#endif
}

void AbbrevSelector::installMatches(AbbrevSelection::Ptr Previous) {
  installDefaults(Previous);
  installIntSeqMatches(Previous);
}

AbbrevSelection::Ptr AbbrevSelector::popHeap() {
  assert(Heap);
  HeapType::entry_ptr Entry = Heap->top();
  Heap->pop();
  return Entry->getValue();
}

AbbrevSelection::Ptr AbbrevSelector::select() {
  TRACE_METHOD("select");
  (void) Flags;
  AbbrevSelection::Ptr Min;
  if (Buffer.size() == 0)
    return Min;
  // Install possible defaults.
  Heap->clear();
  installMatches();
  while (!Heap->empty()) {
    AbbrevSelection::Ptr Sel = popHeap();
    TRACE_ABBREV_SELECTION("Select", Sel);
    assert(bool(Sel));
    if (Sel->getBufferIndex() == Buffer.size()) {
      if (!bool(Min) || Min->compare(Sel.get()) > 0) {
        Min = Sel;
        TRACE_MESSAGE("Define as minimum");
      } else {
        TRACE_MESSAGE("Ignoring: not minimum");
      }
      continue;
    }
    installMatches(Sel);
  }
  return Min;
}

}  // end of namespace intcomp

}  // end of namespace wasm
