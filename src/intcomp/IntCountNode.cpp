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

size_t CountNode::getWeight() const {
  return Count;
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

int CountNode::Ptr::compare(const CountNode::Ptr& P) const {
  if (NdPtr == nullptr)
    return P.NdPtr == nullptr ? 0 : -1;
  if (P.NdPtr == nullptr)
    return 1;
  return NdPtr->compare(*P.NdPtr);
}

void CountNode::Ptr::describe(FILE* File) {
  if (NdPtr)
    NdPtr->describe(File);
  else
    fprintf(File, "nullptr\n");
}

void CountNode::indent(FILE* Out, size_t NestLevel, bool AddWeight) const {
  for (size_t i = 0; i < NestLevel; ++i)
    fputs("  ", Out);
  if (AddWeight)
    fprintf(Out, "%12" PRIuMAX "", uintmax_t(getWeight()));
}

BlockCountNode::~BlockCountNode() {
}

void BlockCountNode::describe(FILE* Out, size_t NestLevel) const {
  indent(Out, NestLevel);
  fputs(": Block\n", Out);
}

int IntCountNode::compare(const CountNode& Nd) const {
  int Diff = CountNode::compare(Nd);
  if (Diff != 0)
    return Diff;
  // Compare structurally.
  const IntCountNode* Arg2 = dyn_cast<IntCountNode>(&Nd);
  if (Arg2 == nullptr)
    return Diff;
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
  indent(Out, NestLevel);
  fputs(": Value", Out);
  if (pathLength() > 1)
    fputc('s', Out);
  fputc(':', Out);
  // TODO(karlschimpf): Make this a programmable parameter.
  describePath(Out, 10);
  fprintf(Out, "  Count: %" PRIuMAX "\n", uintmax_t(getCount()));
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
                                   IntType Value,
                                   IntCountNode* Parent) {
  CountNode* CntNd = UsageMap[Value];
  IntCountNode* Nd = dyn_cast<IntCountNode>(CntNd);
  if (Nd == nullptr) {
    if (Parent)
      Nd = new IntSeqCountNode(Value, Parent);
    else
      Nd = new SingletonCountNode(Value);
    UsageMap[Value] = Nd;
  }
  return Nd;
}

void IntCountNode::clear(IntCountUsageMap& UsageMap) {
  for (auto& pair : UsageMap)
    delete pair.second;
  UsageMap.clear();
}

SingletonCountNode::SingletonCountNode(IntType Value)
    : IntCountNode(Kind::Singleton, Value, nullptr) {
}

SingletonCountNode::~SingletonCountNode() {
}

IntSeqCountNode::IntSeqCountNode(IntType Value, IntCountNode* Parent)
    : IntCountNode(Kind::IntSequence, Value, Parent) {
  assert(Parent);
}

IntSeqCountNode::~IntSeqCountNode() {
  clear(NextUsageMap);
}

}  // end of namespace intcomp

}  // end of namespace wasm
