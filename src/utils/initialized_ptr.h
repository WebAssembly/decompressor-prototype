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

// Defines pointers that are always initialized.

#ifndef DECOMPRESSOR_SRC_UTILS_INITIALIZED_PTR_H
#define DECOMPRESSOR_SRC_UTILS_INITIALIZED_PTR_H

namespace wasm {

namespace utils {

template <typename T>
class initialized_ptr {
 public:
  typedef T* pointer;
  typedef T element_type;
  initialized_ptr() : Ptr(nullptr) {}
  initialized_ptr(std::nullptr_t) : Ptr(nullptr) {}
  explicit initialized_ptr(const initialized_ptr<T>& P) : Ptr(P.Ptr) {}
  explicit initialized_ptr(pointer P) : Ptr(P) {}
  initialized_ptr<T>& operator=(initialized_ptr<T>& P) {
    Ptr = P.Ptr;
    return *this;
  }
  initialized_ptr<T>& operator=(pointer P) {
    Ptr = P;
    return *this;
  }
  initialized_ptr<T>& operator=(std::nullptr_t) {
    Ptr = nullptr;
    return *this;
  }
  void reset(pointer P = nullptr) { Ptr = P; }
  void swap(initialized_ptr<T>& P) {
    pointer Tmp = Ptr;
    Ptr = P.Ptr;
    P.Poitner = Tmp;
  }
  void swap(pointer& P) {
    pointer Tmp = Ptr;
    Ptr = P;
    P = Tmp;
  }
  pointer get() const { return Ptr; }
  explicit operator bool() const { return Ptr != nullptr; }
  pointer operator->() const { return Ptr; }
  const element_type& operator*() const { return *Ptr; }
  element_type& operator*() { return *Ptr; }

 private:
  T* Ptr;
};

template <typename T>
inline bool operator<(const initialized_ptr<T>& P1,
                      const initialized_ptr<T>& P2) {
  return P1.get() < P2.get();
}

template <typename T>
inline bool operator<(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr < P2.get();
}

template <typename T>
inline bool operator<(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() < nullptr;
}

template <typename T>
inline bool operator<=(const initialized_ptr<T>& P1,
                       const initialized_ptr<T>& P2) {
  return P1.get() <= P2.get();
}

template <typename T>
inline bool operator<=(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr <= P2.get();
}

template <typename T>
inline bool operator<=(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() <= nullptr;
}

template <typename T>
inline bool operator==(const initialized_ptr<T>& P1,
                       const initialized_ptr<T>& P2) {
  return P1.get() == P2.get();
}

template <typename T>
inline bool operator==(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr == P2.get();
}

template <typename T>
inline bool operator==(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() == nullptr;
}

template <typename T>
inline bool operator>=(const initialized_ptr<T>& P1,
                       const initialized_ptr<T>& P2) {
  return P1.get() >= P2.get();
}

template <typename T>
inline bool operator>=(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr >= P2.get();
}

template <typename T>
inline bool operator>=(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() >= nullptr;
}

template <typename T>
inline bool operator>(const initialized_ptr<T>& P1,
                      const initialized_ptr<T>& P2) {
  return P1.get() > P2.get();
}

template <typename T>
inline bool operator>(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr > P2.get();
}

template <typename T>
inline bool operator>(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() > nullptr;
}

template <typename T>
inline bool operator!=(const initialized_ptr<T>& P1,
                       const initialized_ptr<T>& P2) {
  return P1.get() != P2.get();
}

template <typename T>
inline bool operator!=(std::nullptr_t, const initialized_ptr<T>& P2) {
  return nullptr != P2.get();
}

template <typename T>
inline bool operator!=(const initialized_ptr<T>& P1, std::nullptr_t) {
  return P1.get() != nullptr;
}

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_INITIALIZED_PTR_H
