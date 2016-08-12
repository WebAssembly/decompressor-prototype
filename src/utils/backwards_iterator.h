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

// Defines a backwards iterator for for loop ranges.

#ifndef DECOMPRESSOR_SRC_UTILS_BACKWARDS_ITERATOR_H
#define DECOMPRESSOR_SRC_UTILS_BACKWARDS_ITERATOR_H

namespace wasm {

namespace utils {

template <typename T>
class backwards {
 public:
  explicit backwards(T& t) : t(t) {}
  typename T::const_reverse_iterator begin() const { return t.rbegin(); }
  typename T::const_reverse_iterator end() const { return t.rend(); }

 private:
  const T& t;
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_BACKWARDS_ITERATOR_H
