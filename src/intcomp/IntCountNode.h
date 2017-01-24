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

// Defines nodes to count usages for abbreviations for blocks, integers,
// and seqeences of integers.

#ifndef DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
#define DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H

#include "interp/IntFormats.h"
#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/heap.h"
#include "utils/HuffmanEncoding.h"

#include <map>
#include <memory>

// WARNING: All CountNode classes should be created using sdt::make_shared!

namespace wasm {

namespace intcomp {

class BlockCountNode;
class CountNode;
class CountNodeWithSuccs;
class DefaultCountNode;
class IntCountNode;
class RootCountNode;

// Generic base class for counting the number of times an input artifact
// appears in a WASM file.
class CountNode : public std::enable_shared_from_this<CountNode> {
  CountNode(const CountNode&) = delete;
  CountNode() = delete;
  CountNode& operator=(const CountNode&) = delete;

 public:
  typedef std::shared_ptr<CountNode> Ptr;
  typedef std::shared_ptr<IntCountNode> IntPtr;
  typedef std::shared_ptr<BlockCountNode> BlockPtr;
  typedef std::shared_ptr<DefaultCountNode> DefaultPtr;
  typedef std::weak_ptr<IntCountNode> ParentPtr;
  typedef std::shared_ptr<RootCountNode> RootPtr;
  typedef std::shared_ptr<CountNodeWithSuccs> WithSuccsPtr;
  typedef std::map<decode::IntType, CountNode::IntPtr> SuccMap;
  typedef std::vector<Ptr> PtrVector;
  typedef std::map<size_t, Ptr> Int2PtrMap;
  typedef SuccMap::const_iterator SuccMapIterator;
  typedef Ptr HeapValueType;
  typedef utils::heap<HeapValueType> HeapType;
  typedef std::shared_ptr<HeapType::entry> HeapEntryType;
  typedef std::function<bool(Ptr, Ptr)> CompareFcnType;

  static const decode::IntType BAD_ABBREV_INDEX;

  static CompareFcnType CompareLt;
  static CompareFcnType CompareGt;

  virtual ~CountNode();
  enum class Kind { Root, Block, Default, Singleton, IntSequence };
  size_t getCount() const { return Count; }
  void setCount(size_t NewValue) { Count = NewValue; }
  size_t getWeight() const { return getWeight(getCount()); }
  virtual size_t getWeight(size_t Count) const;
  void increment(size_t Cnt = 1) { Count += Cnt; }

  // The following two handle associating a heap entry with this.
  void associateWithHeap(HeapEntryType Entry) { HeapEntry = Entry; }
  void disassociateFromHeap() { HeapEntry.reset(); }

  static bool isAbbrevDefined(decode::IntType Abbrev) {
    return Abbrev != BAD_ABBREV_INDEX;
  }
  decode::IntType getAbbrevIndex() const;
  bool hasAbbrevIndex() const;
  void clearAbbrevIndex();
  void setAbbrevIndex(utils::HuffmanEncoder::SymbolPtr Symbol);

  virtual int compare(const CountNode& Nd) const;
  bool operator<(const CountNode& Nd) const { return compare(Nd) < 0; }
  bool operator<=(const CountNode& Nd) const { return compare(Nd) <= 0; }
  bool operator==(const CountNode& Nd) const { return compare(Nd) == 0; }
  bool operator!=(const CountNode& Nd) const { return compare(Nd) != 0; }
  bool operator>=(const CountNode& Nd) const { return compare(Nd) >= 0; }
  bool operator>(const CountNode& Nd) const { return compare(Nd) > 0; }

  virtual void describe(FILE* out, size_t NestLevel = 0) const = 0;

  // The following define casting operations isa<>, dyn_cast<>, and cast<>.
  Kind getRtClassId() const { return NodeKind; }
  static bool implementsClass(Kind /*NodeKind*/) { return true; }

 protected:
  Kind NodeKind;
  size_t Count;
  utils::HuffmanEncoder::SymbolPtr AbbrevSymbol;

  // The heap position of this, when added to heap. Note: Used to
  // allow the ability to change the priority key (i.e. weight) while
  // it is on the heap.
  HeapEntryType HeapEntry;

  CountNode(Kind NodeKind)
      : NodeKind(NodeKind), Count(0) {}
  // The following two enclose description entries.
  void indent(FILE* Out, size_t NestLevel, bool AddWeight = true) const;
  void newline(FILE* Out) const;
};

CountNode::IntPtr lookup(CountNode::RootPtr, decode::IntType Value);
CountNode::IntPtr lookup(CountNode::IntPtr, decode::IntType Value);

int compare(CountNode::Ptr P1, CountNode::Ptr P2);

inline bool operator<(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) < 0;
}

inline bool operator<=(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) <= 0;
}

inline bool operator==(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) == 0;
}

inline bool operator>=(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) >= 0;
}

inline bool operator>(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) > 0;
}

inline bool operator!=(CountNode::Ptr Nd1, CountNode::Ptr Nd2) {
  return compare(Nd1, Nd2) != 0;
}

class CountNodeWithSuccs : public CountNode {
  CountNodeWithSuccs() = delete;
  CountNodeWithSuccs(const CountNodeWithSuccs&) = delete;
  CountNodeWithSuccs& operator=(const CountNodeWithSuccs&) = delete;
  friend CountNode::IntPtr lookup(CountNode::RootPtr, decode::IntType Value);
  friend CountNode::IntPtr lookup(CountNode::IntPtr, decode::IntType Value);

 public:
  ~CountNodeWithSuccs() OVERRIDE;
  SuccMapIterator getSuccBegin() const { return Successors.begin(); }
  SuccMapIterator getSuccEnd() const { return Successors.end(); }
  bool hasSuccessors() const { return !Successors.empty(); }
  void clearSuccs() { Successors.clear(); }
  CountNode::IntPtr getSucc(decode::IntType V);
  void eraseSucc(decode::IntType V) { Successors.erase(V); }
  static bool implementsClass(Kind K);

 protected:
  SuccMap Successors;
  CountNodeWithSuccs(Kind NodeKind) : CountNode(NodeKind) {}
};

// Implements a counter of the number of blocks in a bitcode file.
class BlockCountNode : public CountNode {
  BlockCountNode(const BlockCountNode&) = delete;
  BlockCountNode& operator=(const BlockCountNode&) = delete;

 public:
  explicit BlockCountNode(bool IsEnter)
      : CountNode(Kind::Block), IsEnter(IsEnter) {}
  ~BlockCountNode() OVERRIDE;
  bool isEnter() const { return IsEnter; }
  bool IsExit() const { return !IsEnter; }
  int compare(const CountNode& Nd) const OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  static bool implementsClass(Kind K) { return K == Kind::Block; }

 private:
  bool IsEnter;
};

// Implements default encodings of integers.
class DefaultCountNode : public CountNode {
  DefaultCountNode(const DefaultCountNode&) = delete;
  DefaultCountNode& operator=(const DefaultCountNode&) = delete;

 public:
  explicit DefaultCountNode(bool IsSingle)
      : CountNode(Kind::Default), IsSingle(IsSingle) {}
  ~DefaultCountNode() OVERRIDE;
  bool isSingle() const { return IsSingle; }
  bool isMultiple() const { return !IsSingle; }
  int compare(const CountNode& Nd) const OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  static bool implementsClass(Kind K) { return K == Kind::Default; }

 private:
  bool IsSingle;
};

class RootCountNode : public CountNodeWithSuccs {
  RootCountNode(const RootCountNode&) = delete;
  RootCountNode& operator=(const RootCountNode&) = delete;

 public:
  RootCountNode();
  ~RootCountNode() OVERRIDE;
  CountNode::BlockPtr getBlockEnter() { return BlockEnter; }
  CountNode::BlockPtr getBlockExit() { return BlockExit; }
  CountNode::DefaultPtr getDefaultSingle() { return DefaultSingle; }
  CountNode::DefaultPtr getDefaultMultiple() { return DefaultMultiple; }
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  int compare(const CountNode& Nd) const OVERRIDE;

  void getOthers(PtrVector& L) const;

  static bool implementsClass(Kind K) { return K == Kind::Root; }

 private:
  CountNode::BlockPtr BlockEnter;
  CountNode::BlockPtr BlockExit;
  CountNode::DefaultPtr DefaultSingle;
  CountNode::DefaultPtr DefaultMultiple;
};

// Node defining an IntValue
class IntCountNode : public CountNodeWithSuccs {
  IntCountNode() = delete;
  IntCountNode(const IntCountNode&) = delete;
  IntCountNode& operator=(const IntCountNode&) = delete;

 public:
  ~IntCountNode() OVERRIDE {}
  int compare(const CountNode& Nd) const OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  size_t getValue() const { return Value; }
  size_t getPathLength() const { return PathLength; }
  CountNode::IntPtr getParent() const { return Parent.lock(); }
  size_t getLocalWeight() const;

  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::IntSequence || NodeKind == Kind::Singleton;
  }

 protected:
  decode::IntType Value;
  CountNode::ParentPtr Parent;
  size_t PathLength;
  mutable size_t LocalWeight;
  IntCountNode(Kind NodeKind, decode::IntType Value)
      : CountNodeWithSuccs(NodeKind),
        Value(Value),
        PathLength(1),
        LocalWeight(0) {}
  IntCountNode(Kind NodeKind, decode::IntType Value, CountNode::IntPtr Parent)
      : CountNodeWithSuccs(NodeKind),
        Value(Value),
        Parent(Parent),
        PathLength(Parent->getPathLength() + 1),
        LocalWeight(0) {}
  virtual void describeValues(FILE* Out) const = 0;
};

class SingletonCountNode : public IntCountNode {
  SingletonCountNode() = delete;
  SingletonCountNode(const SingletonCountNode&) = delete;
  SingletonCountNode& operator=(const SingletonCountNode&) = delete;

 public:
  SingletonCountNode(decode::IntType Value)
      : IntCountNode(Kind::Singleton, Value) {}
  ~SingletonCountNode() OVERRIDE;
  size_t getWeight(size_t Count) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::Singleton;
  }

 protected:
  void describeValues(FILE* Out) const OVERRIDE;
};

class IntSeqCountNode : public IntCountNode {
  IntSeqCountNode() = delete;
  IntSeqCountNode(const IntSeqCountNode&) = delete;
  IntSeqCountNode& operator=(const IntSeqCountNode&) = delete;

 public:
  IntSeqCountNode(decode::IntType Value, CountNode::IntPtr Parent)
      : IntCountNode(Kind::IntSequence, Value, Parent) {}

  ~IntSeqCountNode() OVERRIDE;
  size_t getWeight(size_t Count) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::IntSequence;
  }

 protected:
  void describeValues(FILE* Out) const OVERRIDE;
};

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

#endif  // DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
