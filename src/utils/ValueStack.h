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

template <typename T>
class ValueStack {
  ValueStack() = delete;
  ValueStack(const ValueStack&) = delete;
  ValueStack& operator=(const ValueStack&) = delete;

 public:
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
      if (Index < Stack->Stack.size())
        return Stack->Stack.at(Index);
      return Stack->Value;
    }

   private:
    const ValueStack<T>* Stack;
    size_t Index;
  };

  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, Stack.size()); }

  ValueStack(T& Value) : Value(Value) {}
  ~ValueStack() {}
  bool empty() const { return Stack.empty(); }
  void push() { Stack.push_back(Value); }
  // Push Value onto stack
  void push(const T& Value) { Stack.push_back(Value); }
  void pop() {
    assert(!Stack.empty());
    Value = Stack.back();
    Stack.pop_back();
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
