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

#include "intcomp/IntFormats.h"
#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/heap.h"

#include <map>
#include <memory>

// WARNING: All CountNode classes should be created using sdt::make_shared!

namespace wasm {

namespace intcomp {

class BlockCountNode;
class CountNode;
class CountNodeWithSuccs;
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
  typedef std::weak_ptr<IntCountNode> ParentPtr;
  typedef std::shared_ptr<RootCountNode> RootPtr;
  typedef std::map<decode::IntType, CountNode::IntPtr> SuccMap;
  typedef SuccMap::const_iterator SuccMapIterator;
  typedef Ptr HeapValueType;
  typedef utils::heap<HeapValueType> HeapType;
  typedef std::shared_ptr<HeapType::entry> HeapEntryType;

  virtual ~CountNode();
  enum class Kind { Root, Block, Singleton, IntSequence };
  size_t getCount() const { return Count; }
  void setCount(size_t NewValue) { Count = NewValue; }
  size_t getWeight() const { return getWeight(getCount()); }
  virtual size_t getWeight(size_t Count) const;
  void increment(size_t Cnt = 1) { Count += Cnt; }

  // The following two handle associating a heap entry with this.
  void associateWithHeap(HeapEntryType Entry) { HeapEntry = Entry; }
  void disassociateFromHeap() { HeapEntry.reset(); }

  decode::IntType getAbbrevIndex() const { return AbbrevIndex; }
  bool hasAbbrevIndex() const { return AbbrevIndex != BAD_ABBREV_INDEX; }
  void setAbbrevIndex(decode::IntType NewValue) { AbbrevIndex = NewValue; }

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
  decode::IntType AbbrevIndex;
  static constexpr decode::IntType BAD_ABBREV_INDEX =
      std::numeric_limits<decode::IntType>::max();
  // The heap position of this, when added to heap. Note: Used to
  // allow the ability to change the priority key (i.e. weight) while
  // it is on the heap.
  HeapEntryType HeapEntry;

  CountNode(Kind NodeKind)
      : NodeKind(NodeKind), Count(0), AbbrevIndex(BAD_ABBREV_INDEX) {}
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
#if 0
  const SuccMap& getSuccessors() const { return Successors; }
#endif
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

class RootCountNode : public CountNodeWithSuccs {
  RootCountNode(const RootCountNode&) = delete;
  RootCountNode& operator=(const RootCountNode&) = delete;

 public:
  RootCountNode();
  ~RootCountNode() OVERRIDE;
  CountNode::BlockPtr getBlockEnter() { return BlockEnter; }
  CountNode::BlockPtr getBlockExit() { return BlockExit; }
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  int compare(const CountNode& Nd) const OVERRIDE;

  static bool implementsClass(Kind K) { return K == Kind::Root; }

 private:
  CountNode::BlockPtr BlockEnter;
  CountNode::BlockPtr BlockExit;
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
      : CountNodeWithSuccs(Kind::IntSequence),
        Value(Value),
        PathLength(1),
        LocalWeight(0) {}
  IntCountNode(Kind NodeKind, decode::IntType Value, CountNode::IntPtr Parent)
      : CountNodeWithSuccs(Kind::IntSequence),
        Value(Value),
        Parent(Parent),
        PathLength(Parent->getPathLength() + 1),
        LocalWeight(0) {}
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
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
