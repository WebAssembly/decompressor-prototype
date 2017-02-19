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

#ifndef DEcOMPRESSOR_SRC_INTCOMP_COUNTNODE_H
#define DEcOMPRESSOR_SRC_INTCOMP_COUNTNODE_H

#include <map>
#include <set>

#include "intcomp/CompressionFlags.h"
#include "utils/heap.h"
#include "utils/HuffmanEncoding.h"

// WARNING: All CountNode classes should be created using sdt::make_shared!

namespace wasm {

namespace intcomp {

class BlockCountNode;
class CountNode;
class CountNodeWithSuccs;
class DefaultCountNode;
class AlignCountNode;
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
  typedef std::shared_ptr<AlignCountNode> AlignPtr;
  typedef std::weak_ptr<IntCountNode> ParentPtr;
  typedef std::shared_ptr<RootCountNode> RootPtr;
  typedef std::shared_ptr<CountNodeWithSuccs> WithSuccsPtr;
  typedef std::map<decode::IntType, CountNode::IntPtr> SuccMap;
  typedef std::vector<Ptr> PtrVector;
  typedef std::set<Ptr> PtrSet;
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

  static utils::HuffmanEncoder::NodePtr assignAbbreviations(
      PtrSet& Assignments,
      const CompressionFlags& Flags);

  static void describeNodes(FILE* Out, PtrSet& Nodes);
  // Consumes the heap, printing out each node as it is consumed.
  static void describeAndConsumeHeap(FILE* Out, HeapType* Heap);

  enum class Kind { Root, Block, Default, Align, Singleton, IntSequence };
  Kind getKind() const { return NodeKind; }
  size_t getCount() const { return Count; }
  void setCount(size_t NewValue) { Count = NewValue; }
  size_t getWeight() const { return getWeight(getCount()); }
  virtual size_t getWeight(size_t Count) const;
  void increment(size_t Cnt = 1) { Count += Cnt; }

  // The following two handle associating a heap entry with this.
  HeapEntryType getAssociatedHeapEntry() { return HeapEntry; }
  void associateWithHeap(HeapEntryType Entry) { HeapEntry = Entry; }
  void disassociateFromHeap();

  static bool isAbbrevDefined(decode::IntType Abbrev) {
    return Abbrev != BAD_ABBREV_INDEX;
  }
  decode::IntType getAbbrevIndex() const;
  utils::HuffmanEncoder::SymbolPtr getAbbrevSymbol() const {
    return AbbrevSymbol;
  }
  size_t getAbbrevNumBits() const;
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

  // Returns true if the node should be kept, assignment the given compression
  // flag assignments.
  virtual bool smallValueKeep(const CompressionFlags&) const;
  virtual bool keep(const CompressionFlags& Flags) const;
  virtual bool keepSingletonsUsingCount(const CompressionFlags& Flags) const;

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

  CountNode(Kind NodeKind) : NodeKind(NodeKind), Count(0) {}

  // The following two enclose description entries.
  void indent(FILE* Out, size_t NestLevel, bool AddWeight = true) const;
  void newline(FILE* Out) const;
};

CountNode::IntPtr lookup(CountNode::RootPtr,
                         decode::IntType Value,
                         bool AddIfNotFound = true);
CountNode::IntPtr lookup(CountNode::IntPtr,
                         decode::IntType Value,
                         bool AddIfNotFound = true);

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
  friend CountNode::IntPtr lookup(CountNode::RootPtr,
                                  decode::IntType Value,
                                  bool AddIfNotFound);
  friend CountNode::IntPtr lookup(CountNode::IntPtr,
                                  decode::IntType Value,
                                  bool AddIfNotFound);

 public:
  ~CountNodeWithSuccs() OVERRIDE;
  SuccMapIterator begin() const { return Successors.begin(); }
  SuccMapIterator end() const { return Successors.end(); }
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

 protected:
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

 protected:
  bool IsSingle;
};

// Used for modeling alignment (used once in codegen) so that the
// same representation can be used for all abbreviations/actions.
class AlignCountNode : public CountNode {
  AlignCountNode(const AlignCountNode&) = delete;
  AlignCountNode& operator=(const AlignCountNode&) = delete;

 public:
  AlignCountNode() : CountNode(Kind::Align) {}
  ~AlignCountNode() OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  static bool implementsClass(Kind K) { return K == Kind::Align; }

 protected:
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
  CountNode::AlignPtr getAlign() { return AlignCount; }
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  int compare(const CountNode& Nd) const OVERRIDE;

  void getOthers(PtrVector& L) const;

  static bool implementsClass(Kind K) { return K == Kind::Root; }

 protected:
  CountNode::BlockPtr BlockEnter;
  CountNode::BlockPtr BlockExit;
  CountNode::DefaultPtr DefaultSingle;
  CountNode::DefaultPtr DefaultMultiple;
  CountNode::AlignPtr AlignCount;
};

// Node defining an IntValue
class IntCountNode : public CountNodeWithSuccs {
  IntCountNode() = delete;
  IntCountNode(const IntCountNode&) = delete;
  IntCountNode& operator=(const IntCountNode&) = delete;

 public:
  ~IntCountNode() OVERRIDE {}
  int compare(const CountNode& Nd) const OVERRIDE;
  bool smallValueKeep(const CompressionFlags& Flags) const OVERRIDE;
  bool keep(const CompressionFlags& Flags) const OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  decode::IntType getValue() const { return Value; }
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
  explicit SingletonCountNode(decode::IntType Value)
      : IntCountNode(Kind::Singleton, Value), SmallValueKeep(false) {}
  ~SingletonCountNode() OVERRIDE;
  bool getSmallValueKeep() const { return SmallValueKeep; }
  void setSmallValueKeep(bool NewValue) { SmallValueKeep = NewValue; }
  size_t getWeight(size_t Count) const OVERRIDE;
  bool smallValueKeep(const CompressionFlags& Flags) const OVERRIDE;
  bool keepSingletonsUsingCount(const CompressionFlags& Flags) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::Singleton;
  }

 protected:
  bool SmallValueKeep;
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

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DEcOMPRESSOR_SRC_INTCOMP_COUNTNODE_H
