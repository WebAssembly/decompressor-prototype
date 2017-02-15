/* -*- C++ -*- */
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

// Defines a heap that also allows fast removal and reinsertion. To do this, a
// (shared) pointer to an (internal) entry is returned to the caller. By using
// the returned entry, a fast (i.e. log n) removal/reinsertion can be
// performed.
//
// Note: Can't use std::make_heap() or a priority queue because we need the
// ability to quickly remove elements as well.

#ifndef DECOMPRESSOR_SRC_UTILS_HEAP_H
#define DECOMPRESSOR_SRC_UTILS_HEAP_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdio.h>
#include <vector>

namespace wasm {

namespace utils {

// NOTE: Uses value_type::operator<() to define ordering of entries in heap.
template <class value_type>
class heap : public std::enable_shared_from_this<heap<value_type>> {
  heap(const heap&) = delete;
  heap& operator=(const heap&) = delete;

 public:
  // Holds entries in the heap.
  class entry : public std::enable_shared_from_this<entry> {
    entry() = delete;
    entry(const entry&) = delete;
    entry& operator=(const entry*) = delete;
    friend class heap<value_type>;

   public:
    entry(std::weak_ptr<heap<value_type>> HeapWeakPtr,
          const value_type& Value,
          size_t Index)
        : HeapWeakPtr(HeapWeakPtr), Value(Value), Index(Index) {}
    ~entry() {}
    value_type getValue() { return Value; }
    bool reinsert() {
      bool Inserted = false;
      if (auto HeapPtr = HeapWeakPtr.lock()) {
        if (HeapPtr->isValidEntry(this)) {
          HeapPtr->reinsert(Index);
          Inserted = true;
        } else {
          HeapWeakPtr.reset();
        }
      }
      return Inserted;
    }
    bool remove() {
      bool Removed = false;
      if (auto HeapPtr = HeapWeakPtr.lock()) {
        if (HeapPtr->isValidEntry(this)) {
          HeapPtr->remove(Index);
          Removed = true;
        } else {
          HeapWeakPtr.reset();
        }
      }
      return Removed;
    }

    bool isValid() {
      if (auto HeapPtr = HeapWeakPtr.lock())
        return HeapPtr->isValidEntry(this);
      return false;
    }

   private:
    std::weak_ptr<heap<value_type>> HeapWeakPtr;
    value_type Value;
    size_t Index;
  };

  typedef std::shared_ptr<entry> entry_ptr;

  typedef std::function<bool(value_type, value_type)> CompareFcn;
  // WARNING: Only create using std::make_shared<heap<value_type>>(); DO NOT
  // call constructor directly!
  heap(CompareFcn LtFcn) : LtFcn(LtFcn) {}

  ~heap() {}

  heap& setLtFcn(CompareFcn NewLtFcn) {
    LtFcn = NewLtFcn;
    return *this;
  }

  bool empty() const { return Contents.empty(); }

  size_t size() const { return Contents.size(); }

  bool isValidEntry(entry* E) {
    if (E->Index >= Contents.size())
      return false;
    return Contents[E->Index].get() == E;
  }

  entry_ptr top() {
    assert(!Contents.empty());
    return Contents.front();
  }

  entry_ptr push(value_type& Value) {
    size_t Index = Contents.size();
    entry_ptr Entry =
        std::make_shared<entry>(this->shared_from_this(), Value, Index);
    Contents.push_back(Entry);
    insertUp(Index);
    return Entry;
  }

  void push(entry* Entry) {
    Entry->Index = Contents.size();
    Contents.push_back(Entry);
    insertUp(Entry->Index);
    return Entry;
  }

  void clear() {
    while (!empty())
      pop();
  }

  void pop() { remove(0); }

  // Reinsert value since key changed.
  void reinsert(size_t Index) {
    assert(Index < Contents.size());
    if (!insertUp(Index))
      insertDown(Index);
  }

  // Note: This operation is provided to make debugging easier.
  void describe(FILE* Out,
                std::function<void(FILE*, value_type)> describe_fcn) {
    fprintf(Out, "*** Heap ***:\n");
    describeSubtree(Out, 0, 0, describe_fcn);
    fprintf(Out, "************:\n");
  }

 private:
  std::vector<entry_ptr> Contents;

  // Accessors defining indices for parent/children.
  size_t getLeftKidIndex(size_t Parent) { return 2 * Parent + 1; }
  size_t getRightKidIndex(size_t Parent) { return 2 * Parent + 2; }
  size_t getParentIndex(size_t Kid) {
    assert(Kid > 0);
    return (Kid - 1) / 2;
  }

  // Move up to parents as necessary. Returns true if moved.
  bool insertUp(size_t KidIndex) {
    bool Moved = false;
    while (KidIndex) {
      size_t ParentIndex = getParentIndex(KidIndex);
      auto& Parent = Contents[ParentIndex];
      auto& Kid = Contents[KidIndex];
      if (!LtFcn(Kid->getValue(), Parent->getValue()))
        return Moved;
      Parent->Index = KidIndex;
      Kid->Index = ParentIndex;
      std::swap(Parent, Kid);
      KidIndex = ParentIndex;
      Moved = true;
    }
    return Moved;
  }

  // Move down to kids as necessary.
  void insertDown(size_t ParentIndex) {
    do {
      size_t LeftKidIndex = getLeftKidIndex(ParentIndex);
      size_t KidIndex = ParentIndex;
      if (LeftKidIndex >= Contents.size())
        return;
      if (LtFcn(Contents[LeftKidIndex]->getValue(),
                Contents[ParentIndex]->getValue()))
        KidIndex = LeftKidIndex;
      size_t RightKidIndex = getRightKidIndex(ParentIndex);
      if (RightKidIndex < Contents.size()) {
        if (LtFcn(Contents[RightKidIndex]->getValue(),
                  Contents[KidIndex]->getValue()))
          KidIndex = RightKidIndex;
      }
      if (KidIndex == ParentIndex)
        return;
      auto& Parent = Contents[ParentIndex];
      auto& Kid = Contents[KidIndex];
      Parent->Index = KidIndex;
      Kid->Index = ParentIndex;
      std::swap(Parent, Kid);
      ParentIndex = KidIndex;
    } while (true);
  }

  void remove(size_t Index) {
    assert(Index < Contents.size());
    if (Contents.size() - 1 == Index)
      return Contents.pop_back();
    auto& Avail = Contents.back();
    Contents[Index] = Avail;
    Avail->Index = Index;
    Contents.pop_back();
    reinsert(Index);
  }

  // Describes subtree rooted at parent.
  void describeSubtree(FILE* Out,
                       size_t Parent,
                       size_t Indent,
                       std::function<void(FILE*, value_type)> describe_fcn) {
    if (Parent >= size())
      return;
    fprintf(Out, "%8" PRIuMAX ": ", uintmax_t(Parent));
    for (size_t i = 0; i < Indent; ++i)
      fputs("  ", Out);
    describe_fcn(Out, Contents[Parent].get()->getValue());
    ++Indent;
    describeSubtree(Out, getLeftKidIndex(Parent), Indent, describe_fcn);
    describeSubtree(Out, getRightKidIndex(Parent), Indent, describe_fcn);
  }

 private:
  CompareFcn LtFcn;
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_HEAP_H
