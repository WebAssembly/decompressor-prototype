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

// Defines nodes to count usages of integers in a WASM module.

#ifndef DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
#define DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H

#include "intcomp/IntFormats.h"
#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/heap.h"

#include <map>
#include <memory>

namespace wasm {

namespace intcomp {

class CountNode;

// Generic base class for counting the number of times an input artifact
// appears in a WASM file.
class CountNode {
  CountNode(const CountNode&) = delete;
  CountNode() = delete;
  CountNode& operator=(const CountNode&) = delete;

 public:
  // Intentionally encapsulate a pointer, so that we can define comparison
  // for use in a heap.
  class Ptr {
   public:
    CountNode* NdPtr;
    Ptr() : NdPtr(nullptr) {}
    Ptr(const Ptr& P) : NdPtr(P.NdPtr) {}
    Ptr(CountNode* NdPtr) : NdPtr(NdPtr) {}
    Ptr& operator=(const Ptr& P) {
      NdPtr = P.NdPtr;
      return *this;
    }
    int compare(const Ptr& Nd) const;
    CountNode* get() { return NdPtr; }
    const CountNode* get() const { return NdPtr; }
    CountNode& operator*() { return *get(); }
    const CountNode& operator*() const { return *get(); }
    CountNode* operator->() { return get(); }
    const CountNode* operator->() const { return get(); }
    bool operator<(const Ptr& Nd) const { return compare(Nd) < 0; }
    bool operator<=(const Ptr& Nd) const { return compare(Nd) <= 0; }
    bool operator==(const Ptr& Nd) const { return compare(Nd) == 0; }
    bool operator!=(const Ptr& Nd) const { return compare(Nd) != 0; }
    bool operator>=(const Ptr& Nd) const { return compare(Nd) >= 0; }
    bool operator>(const Ptr& Nd) const { return compare(Nd) > 0; }
    void describe(FILE* File);
  };
  typedef Ptr HeapValueType;
  typedef utils::heap<HeapValueType> HeapType;
  typedef std::shared_ptr<HeapType::entry> HeapEntryType;
  virtual ~CountNode();
  enum class Kind { Block, Singleton, IntSequence };
  size_t getCount() const { return Count; }
  size_t getWeight() const { return getWeight(getCount()); }
  virtual size_t getWeight(size_t Count) const;
  void increment(size_t Cnt = 1) { Count += Cnt; }

  // The following two handle associating a heap entry with this.
  void associateWithHeap(HeapEntryType Entry) { HeapEntry = Entry; }
  void disassociateFromHeap() { HeapEntry.reset(); }

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
  // The heap position of this, when added to heap. Note: Used to
  // allow the ability to change the priority key (i.e. weight) while
  // it is on the heap.
  HeapEntryType HeapEntry;

  CountNode(Kind NodeKind) : NodeKind(NodeKind), Count(0) {}
  void indent(FILE* Out, size_t NestLevel, bool AddWeight = true) const;
};

// Implements a counter of the number of blocks in a bitcode file.
class BlockCountNode : public CountNode {
  BlockCountNode(const BlockCountNode&) = delete;
  BlockCountNode& operator=(const BlockCountNode&) = delete;

 public:
  explicit BlockCountNode() : CountNode(Kind::Block) {}
  ~BlockCountNode() OVERRIDE;
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) { return NodeKind == Kind::Block; }
};

typedef std::map<decode::IntType, CountNode*> IntCountUsageMap;

// Implements a notion of a trie on value usage counts.
class IntCountNode : public CountNode {
  IntCountNode() = delete;
  IntCountNode(const IntCountNode&) = delete;
  IntCountNode& operator=(const IntCountNode&) = delete;

 public:
  ~IntCountNode() OVERRIDE {}
  size_t getValue() const { return Value; }
  void describe(FILE* Out, size_t NestLevel = 0) const OVERRIDE;
  size_t pathLength() const;
  bool isSingletonPath() const { return Parent == nullptr; }
  // Lookup (or create if necessary), the entry for Value in UsageMap. Uses
  // Parent value to define parent.
  static IntCountNode* lookup(IntCountUsageMap& LocalUsageMap,
                              IntCountUsageMap& TopLevelUsageMap,
                              decode::IntType Value,
                              IntCountNode* Parent);
  IntCountUsageMap& getNextUsageMap() { return NextUsageMap; }
  // Removes all entries in usage map, deleting unreachable count nodes.
  static void clear(IntCountUsageMap& UsageMap);
  // Removes the given key from the usage map, deleting unreachable count nodes.
  static void erase(IntCountUsageMap& UsageMap, decode::IntType Key) {
    delete UsageMap[Key];
    UsageMap.erase(Key);
  }

  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::IntSequence || NodeKind == Kind::Singleton;
  }

 protected:
  IntCountNode(Kind NodeKind, decode::IntType Value, IntCountNode* Parent)
      : CountNode(Kind::IntSequence), Value(Value), Parent(Parent) {}
  decode::IntType Value;
  IntCountUsageMap NextUsageMap;
  IntCountNode* Parent;
  void describePath(FILE* Out, size_t MaxPath) const;
  int compare(const CountNode& Nd) const OVERRIDE;
};

class SingletonCountNode : public IntCountNode {
  SingletonCountNode() = delete;
  SingletonCountNode(const SingletonCountNode&) = delete;
  SingletonCountNode& operator=(const SingletonCountNode&) = delete;

 public:
  SingletonCountNode(decode::IntType);
  ~SingletonCountNode() OVERRIDE;
  size_t getWeight(size_t Count) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::Singleton;
  }
  int getByteSize(IntTypeFormat Fmt) const { return Formats.getByteSize(Fmt); }
  int getMinByteSize() const { return Formats.getMinFormatSize(); }

 private:
  IntTypeFormats Formats;
};

class IntSeqCountNode : public IntCountNode {
  IntSeqCountNode() = delete;
  IntSeqCountNode(const IntSeqCountNode&) = delete;
  IntSeqCountNode& operator=(const IntSeqCountNode&) = delete;

 public:
  IntSeqCountNode(IntCountUsageMap& TopLevelUsageMap,
                  decode::IntType Value,
                  IntCountNode* Parent);
  ~IntSeqCountNode() OVERRIDE;
  size_t getWeight(size_t Count) const OVERRIDE;
  static bool implementsClass(Kind NodeKind) {
    return NodeKind == Kind::IntSequence;
  }

 private:
  IntCountUsageMap& TopLevelUsageMap;
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
