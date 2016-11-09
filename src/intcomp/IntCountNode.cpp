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
  // implies more data.
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

IntCountNode::IntCountNode(IntType Value, IntCountNode* Parent)
    : CountNode(Kind::IntSequence), Count(0), Value(Value), Parent(Parent) {}

IntCountNode::~IntCountNode() {
  clear(NextUsageMap);
}

size_t IntCountNode::getWeight() const {
  size_t Len = pathLength();
  if (Len == 0)
    Len = 1;
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
  IntCountNode* Nd = UsageMap[Value];
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
