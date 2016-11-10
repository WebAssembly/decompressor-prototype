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

#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/heap.h"

#include <map>
#include <memory>

namespace wasm {

namespace intcomp {

class CountNode;

// Intentionally encapsulated pointer, so that we can define comparison.
class CountNodePtr {
 public:
  CountNode* Ptr;
  CountNodePtr() : Ptr(nullptr) {}
  CountNodePtr(const CountNodePtr& Nd) : Ptr(Nd.Ptr) {}
  CountNodePtr(CountNode* Ptr) : Ptr(Ptr) {}
  CountNodePtr& operator=(const CountNodePtr& Nd) {
    Ptr = Nd.Ptr;
    return *this;
  }
  int compare(const CountNodePtr& Nd) const;
  CountNode* get() { return Ptr; }
  const CountNode* get() const { return Ptr; }
  CountNode& operator*() { return *get(); }
  const CountNode& operator*() const { return *get(); }
  CountNode* operator->() { return get(); }
  const CountNode* operator->() const { return get(); }
  bool operator<(const CountNodePtr& Nd) const { return compare(Nd) < 0; }
  bool operator<=(const CountNodePtr& Nd) const { return compare(Nd) <= 0; }
  bool operator==(const CountNodePtr& Nd) const { return compare(Nd) == 0; }
  bool operator!=(const CountNodePtr& Nd) const { return compare(Nd) != 0; }
  bool operator>=(const CountNodePtr& Nd) const { return compare(Nd) >= 0; }
  bool operator>(const CountNodePtr& Nd) const { return compare(Nd) > 0; }
};

// Generic base class for counting the number of times an input artifact
// appears in a WASM file.
class CountNode {
  CountNode(const CountNode&) = delete;
  CountNode() = delete;
  CountNode& operator=(const CountNode&) = delete;
 public:
  typedef CountNodePtr HeapValueType;
  typedef utils::heap<HeapValueType, size_t> HeapType;
  typedef std::shared_ptr<HeapType::entry> HeapEntryType;
  virtual ~CountNode();
  enum class Kind {
    IntSequence,
    Block,
    FormatUsingI64,
    FormatUsingU64,
    FormatUsingUint8
  };
  size_t getHeapKey() const { return getWeight(); }
  size_t getCount() const { return Count; }
  virtual size_t getWeight() const;
  void increment(size_t Cnt=1) { Count += Cnt; }

  // The following two handle associating a heap entry with this.
  void associateWithHeap(HeapEntryType& Entry) {
    HeapEntry = Entry;
  }
  void disassociateFromHeap() {
    HeapEntry.reset();
  }

  int compare(const CountNode& Nd) const;
  bool operator<(const CountNode& Nd) const {
    return compare(Nd) < 0;
  }
  bool operator<=(const CountNode& Nd) const {
    return compare(Nd) <= 0;
  }
  bool operator==(const CountNode& Nd) const {
    return compare(Nd) == 0;
  }
  bool operator!=(const CountNode& Nd) const {
    return compare(Nd) != 0;
  }
  bool operator>=(const CountNode& Nd) const {
    return compare(Nd) >= 0;
  }
  bool operator>(const CountNode& Nd) const {
    return compare(Nd) > 0;
  }

  virtual void describe(FILE* out, size_t NestLevel=0) const = 0;

  // The following define casting operations isa<>, dyn_cast<>, and cast<>.
  Kind getRtClassId() const {
    return NodeKind;
  }
  static bool implementsClass(Kind NdKind) { return true; }
 protected:
  Kind NodeKind;
  size_t Count;
  // The heap position of this, when added to heap. Note: Used to
  // allow the ability to change the priority key (i.e. weight) while
  // it is on the heap.
  HeapEntryType HeapEntry;

  CountNode(Kind NodeKind) : NodeKind(NodeKind), Count(0) {}
  virtual int compareLocal(const CountNode& Nd) const;
};

typedef std::map<decode::IntType, CountNode*> IntCountUsageMap;

// Implements a notion of a trie on value usage counts.
class IntCountNode : public CountNode {
  IntCountNode(const IntCountNode&) = delete;
  IntCountNode& operator=(const IntCountNode&) = delete;
 public:
  explicit IntCountNode(decode::IntType Value, IntCountNode* Parent = nullptr);
  ~IntCountNode();
  size_t getValue() const { return Value; }
  size_t getWeight() const OVERRIDE;
  void describe(FILE* Out, size_t NestLevel=0) const OVERRIDE;
  size_t pathLength() const;
  bool isSingletonPath() const { return Parent == nullptr; }
  // Lookup (or create if necessary), the entry for Value in UsageMap. Uses
  // Parent value to define parent.
  static IntCountNode* lookup(IntCountUsageMap& UsageMap,
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

  static bool implementsClass(Kind NdKind) { return NdKind == Kind::IntSequence; }
 private:
  decode::IntType Value;
  IntCountUsageMap NextUsageMap;
  IntCountNode* Parent;
  void describePath(FILE* Out, size_t MaxPath) const;
  int compareLocal(const CountNode& Nd) const OVERRIDE;
};

} // end of namespace intcomp

} // end of namespace wasm


#endif // DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
