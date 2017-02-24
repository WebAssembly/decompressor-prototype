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

// Define template bodies used by Ast.h

#ifndef DECOMPRESSOR_SRC_SEXP_AST_TEMPLATES_H_
#define DECOMPRESSOR_SRC_SEXP_AST_TEMPLATES_H_

namespace wasm {

namespace filt {

template <typename T>
T* SymbolTable::create() {
  T* Nd = new T(*this);
  Allocated.push_back(Nd);
  return Nd;
}

template <typename T>
T* SymbolTable::create(Node* Kid) {
  T* Nd = new T(*this, Kid);
  Allocated.push_back(Nd);
  return Nd;
}

template <typename T>
T* SymbolTable::create(Node* Kid1, Node* Kid2) {
  T* Nd = new T(*this, Kid1, Kid2);
  Allocated.push_back(Nd);
  return Nd;
}

template <typename T>
T* SymbolTable::create(Node* Kid1, Node* Kid2, Node* Kid3) {
  T* Nd = new T(*this, Kid1, Kid2, Kid3);
  Allocated.push_back(Nd);
  return Nd;
}

}  // end of namespace filt

}  // end of namespace wasm


#endif // DECOMPRESSOR_SRC_SEXP_AST_TEMPLATES_H_
