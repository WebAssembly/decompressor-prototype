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

// Defines a (non-empty) stack of values.

#ifndef DECOMPRESSOR_SRC_UTILS_VALUESTACK_H
#define DECOMPRESSOR_SRC_UTILS_VALUESTACK_H

#include <vector>

namespace wasm {

namespace utils {

template <typename T>
class ValueStack {
  ValueStack(const ValueStack&) = delete;
  ValueStack& operator=(const ValueStack&) = delete;

 public:
  ValueStack() {}
  ValueStack(const T& Value) : Top(Value) {}
  ~ValueStack() {}
  bool empty() const { return Stack.empty(); }
  const T& get() const { return Top; }
  void set(const T& Value) { Top = Value; }
  // Push top onto stack.
  void push() { Stack.push_back(Top); }
  void push(const T& Value) {
    push();
    Top = Value;
  }
  void pop() {
    assert(!Stack.empty());
    Top = Stack.back();
    Stack.pop_back();
  }
  void reserve(size_t Size) {
    Stack.reserve(Size);
  }
 protected:
  T Top;
  std::vector<T> Stack;
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_VALUESTACK_H
