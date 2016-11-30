/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Define a simple API for implementing runtime casting without using RTTI.
//
// Assumes that you implement a mapping from classes to integers, and
// define the following for castable classes C:
//
// template<class IdType>
// class C ... {
//
// public:
//   IdType getRtClassId() const;
//   static bool implementsClass(IdType RtClassId);
//
// };
//
// WARNING: Only works on classes with single inheritance.
// WARNING: Only works for classes with non-overlapping IdType.

#ifndef DECOMPRESSOR_SRC_CASTING_H
#define DECOMPRESSOR_SRC_CASTING_H

namespace wasm {

// Returns true if N points to an instance of WantedClass
template <class WantedClass, class TestClass>
bool isa(TestClass* N) {
  return WantedClass::implementsClass(N->getRtClassId());
}

template <class WantedClass, class TestClass>
bool isa(const TestClass* N) {
  return WantedClass::implementsClass(N->getRtClassId());
}

// Cast N (no type checking) to type T*.
template <class WantedClass, class TestClass>
WantedClass* cast(TestClass* N) {
  return reinterpret_cast<WantedClass*>(N);
}

template <class WantedClass, class TestClass>
const WantedClass* cast(const TestClass* N) {
  return reinterpret_cast<WantedClass*>(const_cast<TestClass*>(N));
}

// Cast to type T. Returns nullptr if unable.
template <class WantedClass, class TestClass>
WantedClass* dyn_cast(TestClass* N) {
  return isa<WantedClass>(N) ? cast<WantedClass>(N) : nullptr;
}

template <class WantedClass, class TestClass>
const WantedClass* dyn_cast(const TestClass* N) {
  return isa<WantedClass>(N) ? cast<WantedClass>(N) : nullptr;
}

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_CASTING_H
