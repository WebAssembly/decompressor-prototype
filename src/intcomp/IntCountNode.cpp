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

// Implements nodes to count usages of integers in a WASM module.

#include "intcomp/IntCountNode.h"

namespace {

int compareBoolean(bool V1, bool V2) {
  if (V1)
    return V2 ? 0 : -1;
  return V2 ? 1 : 0;
}

template <class T>
int compareValues(T V1, T V2) {
  if (V1 < V2)
    return -1;
  if (V2 < V1)
    return 1;
  return 0;
}

template <class T>
int comparePointers(std::shared_ptr<T> V1, std::shared_ptr<T> V2) {
  if (V1) {
    if (V2)
      return V1->compare(*V2);
    return 1;
  }
  return -1;
}

}  // end of anonymous namespace

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace intcomp {

// NOTE(karlschimpf): Created here because g++ version on Travis not happy
// when used in initializer to heap.
std::function<bool(CountNode::Ptr, CountNode::Ptr)> CountNode::CompareLt =
    [](CountNode::Ptr V1, CountNode::Ptr V2) { return V1 < V2; };

std::function<bool(CountNode::Ptr, CountNode::Ptr)> CountNode::CompareGt =
    [](CountNode::Ptr V1, CountNode::Ptr V2) { return V1 > V2; };

const decode::IntType CountNode::BAD_ABBREV_INDEX =
    std::numeric_limits<decode::IntType>::max();

CountNode::~CountNode() {
  disassociateFromHeap();
}

void CountNode::disassociateFromHeap() {
  if (!HeapEntry)
    return;
  HeapEntry->remove();
  HeapEntry.reset();
}

size_t CountNode::getWeight(size_t Count) const {
  return Count;
}

decode::IntType CountNode::getAbbrevIndex() const {
  if (!AbbrevSymbol)
    return 0;
  return AbbrevSymbol->getPath();
}

bool CountNode::hasAbbrevIndex() const {
  return bool(AbbrevSymbol);
}

void CountNode::clearAbbrevIndex() {
  AbbrevSymbol.reset();
}

void CountNode::setAbbrevIndex(HuffmanEncoder::SymbolPtr Symbol) {
  AbbrevSymbol = Symbol;
}

CountNode::IntPtr lookup(CountNode::RootPtr Root,
                         IntType Value,
                         bool AddIfNotFound) {
  CountNode::IntPtr Succ = Root->getSucc(Value);
  if (Succ)
    return Succ;

  if (!AddIfNotFound)
    return CountNode::IntPtr();

  Succ = std::make_shared<SingletonCountNode>(Value);
  Root->Successors[Value] = Succ;
  return Succ;
}

CountNode::IntPtr lookup(CountNode::IntPtr Nd,
                         IntType Value,
                         bool AddIfNotFound) {
  CountNode::IntPtr Succ = Nd->getSucc(Value);
  if (Succ)
    return Succ;

  if (!AddIfNotFound)
    return CountNode::IntPtr();

  Succ = std::make_shared<IntSeqCountNode>(Value, Nd);
  Nd->Successors[Value] = Succ;
  return Succ;
}

int CountNode::compare(const CountNode& Nd) const {
  // Push ones with highest weight first.
  int Diff = compareValues(getWeight(), Nd.getWeight());
  if (Diff != 0)
    return -Diff;
  // Note: If tie on weight, choose one with smaller count, assuming that
  // implies more data (i.e. weight per element).
  Diff = compareValues(Count, Nd.Count);
  if (Diff)
    return Diff;
  return compareValues(int(NodeKind), int(Nd.NodeKind));
}

bool CountNode::keep(const CompressionFlags& Flags) const {
  return true;
}

bool CountNode::keepSingletonsUsingCount(const CompressionFlags& Flags) const {
  return keep(Flags);
}

void CountNode::indent(FILE* Out, size_t NestLevel, bool AddWeight) const {
  for (size_t i = 0; i < NestLevel; ++i)
    fputs("  ", Out);
  if (AddWeight)
    fprintf(Out, "%12" PRIuMAX "", uintmax_t(getWeight()));
}

void CountNode::newline(FILE* Out) const {
  fprintf(Out, " - Count: %" PRIuMAX "", uintmax_t(getCount()));
  if (hasAbbrevIndex())
    fprintf(Out, " Abbrev: %" PRIuMAX "", uintmax_t(getAbbrevIndex()));
  if (AbbrevSymbol && AbbrevSymbol->getNumBits())
    // Assume binary encoded, so add binary encoding.
    fprintf(Out, " -> 0x%" PRIxMAX ":%u", uintmax_t(AbbrevSymbol->getPath()),
            AbbrevSymbol->getNumBits());
  fputc('\n', Out);
}

int compare(CountNode::Ptr N1, CountNode::Ptr N2) {
  return comparePointers(N1, N2);
}

CountNodeWithSuccs::~CountNodeWithSuccs() {
}

CountNode::IntPtr CountNodeWithSuccs::getSucc(IntType Value) {
  if (Successors.count(Value))
    return Successors[Value];
  return CountNode::IntPtr();
}

bool CountNodeWithSuccs::implementsClass(Kind K) {
  switch (K) {
    case Kind::Root:
    case Kind::Singleton:
    case Kind::IntSequence:
      return true;
    case Kind::Block:
    case Kind::Default:
    case Kind::Align:
      return false;
  }
  WASM_RETURN_UNREACHABLE(false);
}

RootCountNode::RootCountNode()
    : CountNodeWithSuccs(Kind::Root),
      BlockEnter(std::make_shared<BlockCountNode>(true)),
      BlockExit(std::make_shared<BlockCountNode>(false)),
      DefaultSingle(std::make_shared<DefaultCountNode>(true)),
      DefaultMultiple(std::make_shared<DefaultCountNode>(false)),
      AlignCount(std::make_shared<AlignCountNode>()) {
}

RootCountNode::~RootCountNode() {
}

void RootCountNode::getOthers(PtrVector& L) const {
  L.push_back(BlockEnter);
  L.push_back(BlockExit);
  L.push_back(DefaultSingle);
  L.push_back(DefaultMultiple);
  L.push_back(AlignCount);
}

void RootCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": Root", Out);
  newline(Out);
}

int RootCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNodeWithSuccs::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<RootCountNode>(Nd));
  const auto* Root = cast<RootCountNode>(Nd);
  Diff = BlockEnter.get()->compare(*Root->BlockEnter);
  if (Diff != 0)
    return Diff;
  Diff = BlockExit.get()->compare(*Root->BlockExit);
  if (Diff != 0)
    return Diff;
  Diff = DefaultSingle.get()->compare(*Root->DefaultSingle);
  if (Diff != 0)
    return Diff;
  return DefaultMultiple.get()->compare(*Root->DefaultMultiple);
}

BlockCountNode::~BlockCountNode() {
}

int BlockCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNode::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<BlockCountNode>(Nd));
  const auto* BlkNd = cast<BlockCountNode>(Nd);
  return compareBoolean(IsEnter, BlkNd->IsEnter);
}

void BlockCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": Block.", Out);
  fputs(IsEnter ? "enter" : "exit", Out);
  newline(Out);
}

DefaultCountNode::~DefaultCountNode() {
}

int DefaultCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNode::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<DefaultCountNode>(Nd));
  const auto* DefaultNd = cast<DefaultCountNode>(Nd);
  return compareBoolean(IsSingle, DefaultNd->IsSingle);
}

void DefaultCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": default.", Out);
  fputs(IsSingle ? "single" : "multiple", Out);
  newline(Out);
}

AlignCountNode::~AlignCountNode() {
}

void AlignCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": align", Out);
  newline(Out);
}

int IntCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNodeWithSuccs::compare(Nd);
  if (Diff != 0)
    return Diff;

  const IntCountNode* ThisNd = this;
  assert(isa<IntCountNode>(Nd));
  const IntCountNode* IntNd = cast<IntCountNode>(Nd);

  while (ThisNd && IntNd) {
    Diff = compareValues(ThisNd->Value, IntNd->Value);
    if (Diff)
      return Diff;
    ThisNd = ThisNd->getParent().get();
    IntNd = IntNd->getParent().get();
  }
  if (ThisNd)
    return 1;
  if (IntNd)
    return -1;
  return 0;
}

bool IntCountNode::keep(const CompressionFlags& Flags) const {
  return getCount() >= Flags.CountCutoff && getWeight() >= Flags.WeightCutoff;
}

size_t IntCountNode::getLocalWeight() const {
  if (LocalWeight == 0) {
    IntTypeFormats Formats(Value);
    LocalWeight = Formats.getMinFormatSize();
  }
  return LocalWeight;
}

void IntCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": ", Out);
  describeValues(Out);
  newline(Out);
}

SingletonCountNode::~SingletonCountNode() {
}

size_t SingletonCountNode::getWeight(size_t Count) const {
  return Count * getLocalWeight();
}

bool SingletonCountNode::keepSingletonsUsingCount(
    const CompressionFlags& Flags) const {
  return getCount() >= Flags.CountCutoff;
}

void SingletonCountNode::describeValues(FILE* Out) const {
  fputs("Value: ", Out);
  fprint_IntType(Out, getValue());
}

IntSeqCountNode::~IntSeqCountNode() {
}

size_t IntSeqCountNode::getWeight(size_t Count) const {
  size_t Weight = 0;
  const IntCountNode* Nd = this;
  while (Nd) {
    Weight += Nd->getLocalWeight() * Count;
    Nd = dyn_cast<IntCountNode>(Nd->getParent().get());
  }
  return Weight;
}

void IntSeqCountNode::describeValues(FILE* Out) const {
  std::vector<const IntCountNode*> Nodes;
  const IntCountNode* Nd = this;
  while (Nd) {
    Nodes.push_back(Nd);
    Nd = Nd->getParent().get();
  }
  fputs("Values:", Out);
  size_t Count = 0;
  size_t ElidedCount = 0;
  for (
      std::vector<const IntCountNode*>::reverse_iterator Iter = Nodes.rbegin(),
                                                         IterEnd = Nodes.rend();
      Iter != IterEnd; ++Iter) {
    ++Count;
    if (Count > 10) {
      ++ElidedCount;
    } else {
      fputc(' ', Out);
      fprint_IntType(Out, (*Iter)->getValue());
    }
  }
  if (ElidedCount)
    fprintf(Out, " ...[%" PRIuMAX "]", ElidedCount);
}

namespace {

const char* StateName[]{"enter", "visiting", "Exit"};

}  // end of anonymous namespace

const char* CountNodeVisitor::getName(State St) {
  return StateName[unsigned(St)];
}

CountNodeVisitor::Frame::~Frame() {
}

void CountNodeVisitor::Frame::describe(FILE* Out) const {
  describePrefix(Out);
  fprintf(stderr, "  %" PRIuMAX "..%" PRIuMAX " [%" PRIuMAX "] %s\n",
          uintmax_t(FirstKid), uintmax_t(LastKid), uintmax_t(CurKid),
          getName(CurState));
  getNode()->describe(Out);
  describeSuffix(Out);
}

void CountNodeVisitor::Frame::describePrefix(FILE* Out) const {
  fputs("<frame", Out);
}

void CountNodeVisitor::Frame::describeSuffix(FILE* Out) const {
  fputs(">\n", Out);
}

CountNodeVisitor::CountNodeVisitor(CountNode::RootPtr Root) : Root(Root) {
}

CountNodeVisitor::FramePtr CountNodeVisitor::getFrame(size_t FirstKid,
                                                      size_t LastKid) {
  return std::make_shared<Frame>(*this, FirstKid, LastKid);
}

CountNodeVisitor::FramePtr CountNodeVisitor::getFrame(CountNode::IntPtr Nd,
                                                      size_t FirstKid,
                                                      size_t LastKid) {
  return std::make_shared<Frame>(*this, Nd, FirstKid, LastKid);
}

void CountNodeVisitor::describe(FILE* Out) const {
  fprintf(stderr, "*** Stack ***\n");
  for (auto Frame : Stack) {
    Frame->describe(Out);
  }
  fprintf(stderr, "*************\n");
}

void CountNodeVisitor::walk() {
  callRoot();
  walkOther();
  while (!Stack.empty()) {
    FramePtr Frame = Stack.back();
    switch (Frame->CurState) {
      case State::Enter: {
        if (Frame->CurKid >= Frame->LastKid) {
          Frame->CurState = State::Visiting;
          break;
        }
        callNode(ToVisit[Frame->CurKid++]);
        break;
      }
      case State::Visiting: {
        Frame->CurState = State::Exit;
        visit(Frame);
        break;
      }
      case State::Exit: {
        while (ToVisit.size() > Frame->FirstKid)
          ToVisit.pop_back();
        Stack.pop_back();
        visitReturn(Frame);
        break;
      }
    }
  }
}

void CountNodeVisitor::walkOther() {
  CountNode::PtrVector L;
  Root->getOthers(L);
  for (CountNode::Ptr Nd : L)
    visitOther(Nd);
}

void CountNodeVisitor::callRoot() {
  size_t FirstKid = ToVisit.size();
  for (auto& Pair : *Root.get())
    ToVisit.push_back(Pair.second);
  Stack.push_back(getFrame(FirstKid, ToVisit.size()));
}

void CountNodeVisitor::callNode(CountNode::IntPtr Nd) {
  size_t FirstKid = ToVisit.size();
  for (auto& Pair : *Nd.get())
    ToVisit.push_back(Pair.second);
  Stack.push_back(getFrame(Nd, FirstKid, ToVisit.size()));
}

void CountNodeVisitor::visit(FramePtr Frame) {
}

void CountNodeVisitor::visitReturn(FramePtr Frame) {
}

void CountNodeVisitor::visitOther(CountNode::Ptr Nd) {
}

}  // end of namespace intcomp

}  // end of namespace wasm
