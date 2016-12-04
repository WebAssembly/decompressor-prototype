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

namespace wasm {

using namespace decode;

namespace intcomp {

CountNode::~CountNode() {
}

size_t CountNode::getWeight(size_t Count) const {
  return Count;
}

CountNode::IntPtr lookup(CountNode::RootPtr Root, IntType Value) {
  CountNode::IntPtr Succ = Root->getSucc(Value);
  if (Succ)
    return Succ;
  Succ = std::make_shared<SingletonCountNode>(Value, Root);
  Root->Successors[Value] = Succ;
  return Succ;
}

CountNode::IntPtr lookup(CountNode::IntPtr Nd, IntType Value) {
  CountNode::IntPtr Succ = Nd->getSucc(Value);
  if (Succ)
    return Succ;
  Succ = std::make_shared<IntSeqCountNode>(Value, Nd, Nd->getRoot());
  Nd->Successors[Value] = Succ;
  return Succ;
}

int CountNode::compare(const CountNode& Nd) const {
  // Push ones with highest count first.
  size_t MyWeight = getWeight();
  size_t NdWeight = Nd.getWeight();
  if (MyWeight < NdWeight)
    return 1;
  if (MyWeight > NdWeight)
    return -1;
  // Note: If tie on weight, choose one with larger count, assuming that
  // implies more data (i.e. weight per element).
  if (Count < Nd.Count)
    return -2;
  if (Count > Nd.Count)
    return 2;
  if (int(NodeKind) < int(Nd.NodeKind))
    return -3;
  if (int(NodeKind) > int(Nd.NodeKind))
    return 3;
  return 0;
}

void CountNode::indent(FILE* Out, size_t NestLevel, bool AddWeight) const {
  for (size_t i = 0; i < NestLevel; ++i)
    fputs("  ", Out);
  if (AddWeight)
    fprintf(Out, "%12" PRIuMAX "", uintmax_t(getWeight()));
}

void CountNode::newline(FILE* Out) const {
  fprintf(Out, " - Count: %" PRIuMAX "", uintmax_t(getCount()));
  if (AbbrevIndex != BAD_ABBREV_INDEX)
    fprintf(Out, " Abbrev: %" PRIuMAX "", uintmax_t(AbbrevIndex));
  fputc('\n', Out);
}

int compare(CountNode::Ptr N1, CountNode::Ptr N2) {
  if (N1) {
    if (N2)
      return N1->compare(*N2);
    return 1;
  }
  return -1;
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
      return true;
    case Kind::Block:
      return false;
    case Kind::Singleton:
      return true;
    case Kind::IntSequence:
      return true;
  }
  WASM_RETURN_UNREACHABLE(false);
}

RootCountNode::RootCountNode()
    : CountNodeWithSuccs(Kind::Root),
      BlockEnter(std::make_shared<BlockCountNode>(true)),
      BlockExit(std::make_shared<BlockCountNode>(false)) {
}

RootCountNode::~RootCountNode() {
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
  assert(BlockEnter);
  assert(BlockExit);
  Diff = BlockEnter.get()->compare(*Root->BlockExit);
  if (Diff != 0)
    return Diff;
  return BlockExit.get()->compare(*Root->BlockExit);
}

BlockCountNode::~BlockCountNode() {
}

int BlockCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNode::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<BlockCountNode>(Nd));
  const auto* BlkNd = cast<BlockCountNode>(Nd);
  if (IsEnter)
    return BlkNd->IsEnter ? 0 : -1;
  else
    return BlkNd->IsEnter ? 1 : 0;
}

void BlockCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": Block.", Out);
  fputs(IsEnter ? "enter" : "exit", Out);
  newline(Out);
}

int IntCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNodeWithSuccs::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<IntCountNode>(Nd));
  const IntCountNode* IntNd = cast<IntCountNode>(Nd);
  if (Value < IntNd->Value)
    return -1;
  if (Value > IntNd->Value)
    return 1;
  return intcomp::compare(getParent(), IntNd->Parent.lock());
}

size_t IntCountNode::getLocalWeight() const {
  if (LocalWeight == 0) {
    IntTypeFormats Formats(Value);
    LocalWeight = Formats.getMinFormatSize();
  }
  return LocalWeight;
}

void IntCountNode::describe(FILE* Out, size_t NestLevel) const {
  std::vector<const IntCountNode*> IntSeq;
  const IntCountNode* Nd = this;
  while (Nd) {
    IntSeq.push_back(Nd);
    Nd = dyn_cast<IntCountNode>(Nd->getParent().get());
  }
  indent(Out, NestLevel);
  fputs(": Value", Out);
  if (getPathLength() > 1)
    fputc('s', Out);
  fputc(':', Out);
  for (std::vector<const IntCountNode*>::reverse_iterator
           Iter = IntSeq.rbegin(),
           EndIter = IntSeq.rend();
       Iter != EndIter; ++Iter) {
    fputc(' ', Out);
    fprint_IntType(Out, (*Iter)->getValue());
  }
  newline(Out);
}

SingletonCountNode::SingletonCountNode(IntType Value, CountNode::RootPtr Root)
    : IntCountNode(Kind::Singleton, Value, Root) {
}

SingletonCountNode::~SingletonCountNode() {
}

size_t SingletonCountNode::getWeight(size_t Count) const {
  return Count * getLocalWeight();
}

IntSeqCountNode::IntSeqCountNode(IntType Value,
                                 CountNode::IntPtr Parent,
                                 CountNode::RootPtr Root)
    : IntCountNode(Kind::IntSequence, Value, Parent, Root) {
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

}  // end of namespace intcomp

}  // end of namespace wasm
