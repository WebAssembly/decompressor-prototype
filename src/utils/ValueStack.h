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

// Defines a stack-backed value.

#ifndef DECOMPRESSOR_SRC_UTILS_VALUESTACK_H
#define DECOMPRESSOR_SRC_UTILS_VALUESTACK_H

#include <vector>

namespace wasm {

namespace utils {

// A stack backed value. That is, top is stored in a local variable, and methods
// push() and pop() move data to/from the stack. Conceptually, top is part of
// the stack.
template <typename T>
class ValueStack {
  ValueStack() = delete;
  ValueStack(const ValueStack&) = delete;
  ValueStack& operator=(const ValueStack&) = delete;

 public:
  // Iterator pointer for iterating over the stack.
  class Iterator {
   public:
    Iterator(const ValueStack<T>* Stack, size_t Index)
        : Stack(Stack), Index(Index) {}
    Iterator(const Iterator& Iter) : Stack(Iter.Stack), Index(Iter.Index) {}
    void operator++() { ++Index; }
    void operator--() { --Index; }
    Iterator& operator=(const Iterator& Iter) {
      Stack = Iter.Stack;
      Index = Iter.Index;
      return *this;
    }
    bool operator==(const Iterator& Iter) {
      return Index == Iter.Index && Stack == Iter.Stack;
    }
    bool operator!=(const Iterator& Iter) {
      return Index != Iter.Index || Stack != Iter.Stack;
    }
    const T& operator*() const {
      if (Index < Stack->size())
        return Stack->Stack.at(Index);
      return Stack->Value;
    }

   private:
    const ValueStack<T>* Stack;
    size_t Index;
  };

  // Iterator for range [BeginIndex, EndIndex).
  // Max range is: [0, sizeWithTop()]
  class IteratorRange {
   public:
    IteratorRange(const ValueStack<T>* Stack,
                  size_t BeginIndex,
                  size_t EndIndex)
        : Stack(Stack), BeginIndex(BeginIndex), EndIndex(EndIndex) {}
    IteratorRange(const ValueStack<T>* Stack, size_t BeginIndex)
        : Stack(Stack),
          BeginIndex(BeginIndex),
          EndIndex(Stack->sizeWithTop()) {}
    Iterator begin() { return Iterator(Stack, BeginIndex); }
    Iterator end() { return Iterator(Stack, EndIndex); }

   private:
    const ValueStack<T>* Stack;
    size_t BeginIndex;
    size_t EndIndex;
  };

  class BackwardIterator {
   public:
    BackwardIterator(const ValueStack<T>* Stack, size_t Index)
        : Stack(Stack), Index(Index) {}
    BackwardIterator(const BackwardIterator& Iter)
        : Stack(Iter.Stack), Index(Iter.Index) {}
    void operator++() { --Index; }
    void operator--() { ++Index; }
    BackwardIterator& operator=(const BackwardIterator& Iter) {
      Stack = Iter.Stack;
      Index = Iter.Index;
      return *this;
    }
    bool operator==(const BackwardIterator& Iter) {
      return Index == Iter.Index && Stack == Iter.Stack;
    }
    bool operator!=(const BackwardIterator& Iter) {
      return Index != Iter.Index || Stack != Iter.Stack;
    }
    const T& operator*() const {
      if (Index < Stack->Stack.size())
        return Stack->Stack.at(Index);
      return Stack->Value;
    }

   private:
    const ValueStack<T>* Stack;
    size_t Index;
  };

  // reverse iterate over range[BeginIndex, EndIndex).
  // Max range is [0, sizeWithTop()).
  class BackwardIteratorRange {
   public:
    BackwardIteratorRange(const ValueStack<T>* Stack,
                          size_t BeginIndex,
                          size_t EndIndex)
        : Stack(Stack), BeginIndex(BeginIndex), EndIndex(EndIndex) {}
    BackwardIteratorRange(const ValueStack<T>* Stack, size_t BeginIndex)
        : Stack(Stack),
          BeginIndex(BeginIndex),
          EndIndex(Stack->sizeWithTop()) {}
    BackwardIterator begin() { return BackwardIterator(Stack, EndIndex - 1); }
    BackwardIterator end() { return BackwardIterator(Stack, BeginIndex - 1); }

   private:
    const ValueStack<T>* Stack;
    size_t BeginIndex;
    size_t EndIndex;
  };

  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, sizeWithTop()); }
  IteratorRange iterRange(size_t BeginIndex, size_t EndIndex) const {
    assert(BeginIndex <= sizeWithTop());
    assert(EndIndex <= sizeWithTop());
    return IteratorRange(this, BeginIndex, EndIndex);
  }
  IteratorRange iterRange(size_t BeginIndex) const {
    assert(BeginIndex <= sizeWithTop());
    return IteratorRange(this, BeginIndex);
  }
  BackwardIterator rbegin() const { return BackwardIterator(this, size()); }
  BackwardIterator rend() const { return BackWardIterator(this, size_t(-1)); }
  BackwardIteratorRange riterRange(size_t BeginIndex, size_t EndIndex) const {
    assert(BeginIndex <= sizeWithTop());
    assert(EndIndex <= sizeWithTop());
    return BackwardIteratorRange(this, BeginIndex, EndIndex);
  }
  BackwardIteratorRange riterRange(size_t BeginIndex) const {
    assert(BeginIndex <= sizeWithTop());
    return BackwardIteratorRange(this, BeginIndex);
  }
  ValueStack(T& Value) : Value(Value) {}
  ~ValueStack() {}
  // Note: Empty doesn't include top.
  bool empty() const { return Stack.empty(); }
  // Note: Size doesn't include top.
  size_t size() const { return Stack.size(); }
  size_t sizeWithTop() const { return Stack.size() + 1; }
  // Note: This assumes that the stack includes the original top (and
  // hence pushed at index zero).
  const T& at(size_t Index) const {
    if (Index < Stack.size())
      return Stack.at(Index);
    assert(Index == Stack.size());
    return Value;
  }
  // Note: This assumes that the stack excludes the original top (and
  // hence, the first real value begins at index 1).
  const T& operator[](size_t Index) const {
    if (++Index < Stack.size())
      return Stack.at(Index);
    assert(Index == Stack.size());
    return Value;
  }
  void push() { Stack.push_back(Value); }
  // Push Value onto stack
  void push(const T& NewValue) {
    push();
    Value = NewValue;
  }
  void pop() {
    assert(!Stack.empty());
    Value = Stack.back();
    Stack.pop_back();
  }
  T popValue() {
    T Result = Value;
    pop();
    return Result;
  }
  void clear() { Stack.clear(); }
  void reserve(size_t Size) { Stack.reserve(Size); }

 protected:
  T& Value;
  std::vector<T> Stack;
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_VALUESTACK_H
