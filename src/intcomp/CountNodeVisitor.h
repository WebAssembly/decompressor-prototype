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

// Defines a visitor class that visits nodes in a count node trie.

#ifndef DEcOMPRESSOR_SRC_INTCOMP_COUNTNODEVISITOR_H
#define DEcOMPRESSOR_SRC_INTCOMP_COUNTNODEVISITOR_H

#include "intcomp/CountNode.h"

// WARNING: All CountNode classes should be created using sdt::make_shared!

namespace wasm {

namespace intcomp {

// Defines a visitor for a (root-based) trie.
class CountNodeVisitor {
  CountNodeVisitor() = delete;
  CountNodeVisitor(const CountNodeVisitor&) = delete;
  CountNodeVisitor& operator=(const CountNodeVisitor&) = delete;

 public:
  class Frame;
  typedef std::shared_ptr<Frame> FramePtr;

  enum class State { Enter, Visiting, Exit };
  static const char* getName(State St);

  class Frame {
    Frame() = delete;
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    friend class CountNodeVisitor;

   public:
    // Create frame referring to root of visitor.
    Frame(CountNodeVisitor& Visitor, size_t FirstKid, size_t LastKid)
        : Visitor(Visitor),
          FirstKid(FirstKid),
          LastKid(LastKid),
          CurKid(FirstKid),
          CurState(State::Enter) {}
    // Create frame referring to Nd of visitor.
    Frame(CountNodeVisitor& Visitor,
          CountNode::IntPtr Nd,
          size_t FirstKid,
          size_t LastKid)
        : Visitor(Visitor),
          FirstKid(FirstKid),
          LastKid(LastKid),
          CurKid(FirstKid),
          CurState(State::Enter),
          Nd(Nd) {}

    virtual ~Frame();

    bool isRootFrame() const { return !bool(Nd); }
    CountNode::RootPtr getRoot() const { return Visitor.getRoot(); }

    bool isIntNodeFrame() const { return bool(Nd); }
    CountNode::IntPtr getIntNode() const {
      assert(isIntNodeFrame());
      return Nd;
    }

    // Returns the node being visited.
    CountNode::WithSuccsPtr getNode() const {
      if (isIntNodeFrame())
        return Nd;
      return getRoot();
    }

    // The following is for debugging.
    void describe(FILE* Out) const;
    virtual void describePrefix(FILE* Out) const;
    virtual void describeSuffix(FILE* Out) const;

   protected:
    CountNodeVisitor& Visitor;
    size_t FirstKid;
    size_t LastKid;
    size_t CurKid;
    State CurState;
    CountNode::IntPtr Nd;
  };

  CountNodeVisitor(CountNode::RootPtr Root);
  ~CountNodeVisitor() {}

  void walk();

  CountNode::RootPtr getRoot() const { return Root; }

  void describe(FILE* Out) const;

 protected:
  CountNode::RootPtr Root;
  std::vector<CountNode::IntPtr> ToVisit;
  std::vector<FramePtr> Stack;

  virtual FramePtr getFrame(size_t FirstKid, size_t LastKid);
  virtual FramePtr getFrame(CountNode::IntPtr Nd,
                            size_t FirstKid,
                            size_t LastKid);

  void walkOther();

  void callRoot();
  void callNode(CountNode::IntPtr Nd);

  virtual void visit(FramePtr Frame);
  // Returns any values to the calling frame (i.e. the topmost frame
  // in the visitor.
  virtual void visitReturn(FramePtr Frame);

  // Visits non-root/int count nodes.
  virtual void visitOther(CountNode::Ptr Nd);
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DEcOMPRESSOR_SRC_INTCOMP_COUNTNODEVISITOR_H
