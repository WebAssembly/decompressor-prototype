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

CountNode::~CountNode() {}

size_t CountNode::getWeight() const { return Count; }

int CountNode::compare(const CountNode& Nd) const {
  size_t MyWeight = getWeight();
  size_t NdWeight = Nd.getWeight();
  if (MyWeight < NdWeight)
    return -1;
  if (MyWeight > NdWeight)
    return 1;
  // Note: If tie on weight, choose one with smaller count, assuming that
  // implies more data (i.e. weight per element).
  if (Count < Nd.Count)
    return 1;
  if (Count > Nd.Count)
    return -1;
  if (int(NodeKind) < int(Nd.NodeKind))
    return -1;
  if (int(NodeKind) > int(Nd.NodeKind))
    return 1;
  return compareLocal(Nd);
}

int CountNode::compareLocal(const CountNode& Nd) const {
  return 0;
}

int CountNodePtr::compare(const CountNodePtr& NdPtr) const {
  if (Ptr == nullptr)
    return NdPtr.Ptr == nullptr ? 0 : -1;
  if (NdPtr.Ptr == nullptr)
    return 1;
  return Ptr->compare(*NdPtr.Ptr);
}

IntCountNode::IntCountNode(IntType Value, IntCountNode* Parent)
    : CountNode(Kind::IntSequence), Value(Value), Parent(Parent) {}

IntCountNode::~IntCountNode() {
  clear(NextUsageMap);
}

int IntCountNode::compareLocal(const CountNode& Nd) const {
  const IntCountNode* Arg2 = dyn_cast<IntCountNode>(&Nd);
  if (Arg2 == nullptr)
    return 1;
  const IntCountNode* Arg1 = this;
  size_t Arg1Len = Arg1->pathLength();
  size_t Arg2Len = Arg2->pathLength();
  if (Arg1Len < Arg2Len)
    return -1;
  if (Arg1Len > Arg2Len)
    return 1;
  while (Arg1) {
    assert(Arg2);
    if (Arg1->Value < Arg2->Value)
      return -1;
    if (Arg1->Value > Arg2->Value)
      return 1;
    Arg1 = Arg1->Parent;
    Arg2 = Arg2->Parent;
  }
  return 0;
}

size_t IntCountNode::getWeight() const {
  size_t Len = pathLength();
  return getCount() * Len;
}

void IntCountNode::describe(FILE* Out, size_t NestLevel) const {
  for (size_t i = 0; i < NestLevel; ++i)
    fputs("  ", Out);
  fputs("Value", Out);
  if (pathLength() > 1)
    fputc('s', Out);
  fputc(':', Out);
  // TODO(karlschimpf): Make this a programmable parameter.
  describePath(Out, 10);
  fprintf(Out, "\n\tWeight: %-12" PRIuMAX "\tCount: %-12" PRIuMAX "\n",
          uintmax_t(getWeight()), uintmax_t(getCount()));
}

void IntCountNode::describePath(FILE* Out, size_t MaxPath) const {
  if (MaxPath == 0) {
    fprintf(Out, " ...[%" PRIuMAX "]", uintmax_t(pathLength()));
    return;
  }
  if (Parent)
    Parent->describePath(Out, MaxPath - 1);
  fprintf(Out, " %" PRIuMAX "", uintmax_t(Value));
}

size_t IntCountNode::pathLength() const {
  size_t len = 0;
  IntCountNode* Nd = const_cast<IntCountNode*>(this);
  while (Nd) {
    ++len;
    Nd = Nd->Parent;
  }
  return len;
}

IntCountNode* IntCountNode::lookup(IntCountUsageMap& UsageMap,
                                   IntType Value, IntCountNode* Parent) {
  CountNode* CntNd = UsageMap[Value];
  IntCountNode* Nd = dyn_cast<IntCountNode>(CntNd);
  if (Nd == nullptr) {
    Nd = new IntCountNode(Value, Parent);
    UsageMap[Value] = Nd;
  }
  return Nd;
}

void IntCountNode::clear(IntCountUsageMap& UsageMap) {
  for (auto& pair : UsageMap)
    delete pair.second;
  UsageMap.clear();
}

} // end of namespace intcomp

} // end of namespace wasm
