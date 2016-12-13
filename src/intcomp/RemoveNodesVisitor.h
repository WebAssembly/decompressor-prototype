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

// Defines a visitor to remove count nodes (i.e. patterns) with small
// usage counts.

#ifndef DECOMPRESSOR_SRC_INTCOMP_REMOVENODESVISITOR_H
#define DECOMPRESSOR_SRC_INTCOMP_REMOVENODESVISITOR_H

#include "intcomp/IntCountNode.h"

namespace wasm {

namespace intcomp {

class RemoveNodesVisitor : public CountNodeVisitor {
  RemoveNodesVisitor() = delete;
  RemoveNodesVisitor(const RemoveNodesVisitor&) = delete;
  RemoveNodesVisitor& operator=(const RemoveNodesVisitor&) = delete;

 public:
  class Frame : public CountNodeVisitor::Frame {
    Frame() = delete;
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    friend class RemoveNodesVisitor;

   public:
    Frame(CountNodeVisitor& Visitor, size_t FirstKid, size_t LastKid)
        : CountNodeVisitor::Frame(Visitor, FirstKid, LastKid) {}

    Frame(CountNodeVisitor& Visitor,
          CountNode::IntPtr Nd,
          size_t FirstKid,
          size_t LastKid)
        : CountNodeVisitor::Frame(Visitor, Nd, FirstKid, LastKid) {
      assert(Nd);
    }

    ~Frame() OVERRIDE {}

    void describeSuffix(FILE* Out) const OVERRIDE;

   private:
    std::vector<CountNode::IntPtr> RemoveSet;
  };
  explicit RemoveNodesVisitor(CountNode::RootPtr Root,
                              size_t CountCutoff,
                              size_t PatternCutoff = 2)
      : CountNodeVisitor(Root),
        CountCutoff(CountCutoff),
        PatternCutoff(PatternCutoff) {}
  ~RemoveNodesVisitor() {}

 protected:
  size_t CountCutoff;
  size_t PatternCutoff;
  FramePtr getFrame(size_t FirstKid, size_t LastKid) OVERRIDE;
  FramePtr getFrame(CountNode::IntPtr Nd,
                    size_t FirstKid,
                    size_t LastKid) OVERRIDE;

  void visit(FramePtr Frame) OVERRIDE;
  void visitReturn(FramePtr Frame) OVERRIDE;
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  //  DECOMPRESSOR_SRC_INTCOMP_REMOVENODESVISITOR_H
