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

IntCountNode::IntCountNode(IntType Value)
    : Count(0), Value(Value) {}

IntCountNode::~IntCountNode() {}

void IntCountNode::describe(FILE* Out, size_t NestLevel) const {
  for (size_t i = 0; i < NestLevel; ++i)
    fputs("  ", Out);
  fputs("Value", Out);
  if (pathLength() > 1)
    fputc('s', Out);
  fputc(':', Out);
  describePath(Out);
  fprintf(Out, " Count: %" PRIuMAX "\n", uintmax_t(Count));
}

void IntCountNode::describePath(FILE* Out, size_t MaxPath) const {
  if (MaxPath == 0)
    return;
  if (std::shared_ptr<IntCountNode> Parent = MyParent.lock())
    Parent->describePath(Out, MaxPath - 1);
  fprintf(Out, " %" PRIuMAX "", uintmax_t(Value));
}

size_t IntCountNode::pathLength(size_t MaxPath) const {
  if  (MaxPath == 0)
    return 1;
  if (std::shared_ptr<IntCountNode> Parent = MyParent.lock())
    return Parent->pathLength(MaxPath - 1);
  return 1;
}

IntCountNode* IntCountNode::add(IntCountUsageMap& UsageMap, IntType Value) {
  if (UsageMap.count(Value) == 0)
    UsageMap[Value] = std::make_shared<IntCountNode>(Value);
  IntCountNode* Nd = UsageMap[Value].get();
  Nd->increment();
  return Nd;
}

IntCountUsageMap* IntCountNode::getNextUsageMap() {
  IntCountUsageMap* Map = NextUsageMap.get();
  if (Map)
    return Map;
  Map = new IntCountUsageMap();
  NextUsageMap.reset(Map);
  return Map;
}

} // end of namespace intcomp

} // end of namespace wasm
