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

void addUsage(IntCountUsageMap& UsageMap, IntType Value) {
  if (UsageMap.count(Value) == 0)
    UsageMap[Value] = std::make_shared<IntCountNode>(Value);
  UsageMap[Value]->increment();
}

} // end of namespace intcomp

} // end of namespace wasm
