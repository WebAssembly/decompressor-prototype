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

// implements a visitor to remove count nodes (i.e. patterns) with small
// usage counts.

#include "intcomp/RemoveNodesVisitor.h"

namespace wasm {

namespace intcomp {

CountNodeVisitor::FramePtr RemoveNodesVisitor::getFrame(size_t FirstKid,
                                                        size_t LastKid) {
  return std::make_shared<RemoveNodesVisitor::Frame>(*this, FirstKid, LastKid);
}

CountNodeVisitor::FramePtr RemoveNodesVisitor::getFrame(CountNode::IntPtr Nd,
                                                        size_t FirstKid,
                                                        size_t LastKid) {
  return std::make_shared<RemoveNodesVisitor::Frame>(*this, Nd, FirstKid,
                                                     LastKid);
}

void RemoveNodesVisitor::visit(FramePtr Frame) {
  auto* F = reinterpret_cast<RemoveNodesVisitor::Frame*>(Frame.get());
  while (!F->RemoveSet.empty()) {
    CountNode::IntPtr Nd = F->RemoveSet.back();
    F->RemoveSet.pop_back();
    if (isa<SingletonCountNode>(*Nd))
      getRoot()->getDefaultSingle()->increment(Nd->getCount());
    F->getNode()->eraseSucc(Nd->getValue());
  }
}

void RemoveNodesVisitor::visitReturn(FramePtr Frame) {
  auto* F = reinterpret_cast<RemoveNodesVisitor::Frame*>(Frame.get());
  if (!F->isIntNodeFrame())
    return;
  CountNode::IntPtr Nd = F->getIntNode();
  bool Keep = KeepSingletonsUsingCount ? Nd->keepSingletonsUsingCount(Flags)
                                       : Nd->keep(Flags);
  if (Nd->hasSuccessors() || Keep) {
    if (ZeroOutSmallNodes && !Keep && Nd->getCount())
      Nd->setCount(0);
    return;
  }
  reinterpret_cast<RemoveNodesVisitor::Frame*>(Stack.back().get())
      ->RemoveSet.push_back(Nd);
}

void RemoveNodesVisitor::Frame::describeSuffix(FILE* Out) const {
  if (!RemoveSet.empty()) {
    fputc('\n', Out);
    for (auto Nd : RemoveSet) {
      fputs("  ", Out);
      Nd->describe(Out);
    }
  }
  CountNodeVisitor::Frame::describeSuffix(Out);
}

}  // end of namespace intcomp

}  // end of namespace wasm
