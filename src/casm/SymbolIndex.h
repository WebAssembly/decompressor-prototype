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

// Defines the section symbol table that holds the set of symbols defined
// in a section.

#ifndef DECOMPRESSOR_SRC_CASM_SYMBOLINDEX_H
#define DECOMPRESSOR_SRC_CASM_SYMBOLINDEX_H

#include <unordered_map>
#include <vector>
#include <memory>

#include "utils/Defs.h"

namespace wasm {

namespace filt {

class Node;
class Algorithm;
class Symbol;
class SymbolTable;

class SymbolIndex {
  SymbolIndex(const SymbolIndex&) = delete;
  SymbolIndex& operator=(const SymbolIndex&) = delete;

 public:
  typedef uint32_t IndexType;
  typedef std::unordered_map<Symbol*, IndexType> SymbolLookupType;
  typedef std::vector<Symbol*> IndexLookupType;
  SymbolIndex(std::shared_ptr<SymbolTable> Symtab);
  ~SymbolIndex() {}
  void installSymbols();
  void addSymbol(const std::string& Name);
  const IndexLookupType& getVector() { return IndexLookup; }
  uint32_t getSymbolIndex(Symbol* Sym);
  IndexType getNumberSymbols() const { return IndexLookup.size(); }
  Symbol* getIndexSymbol(IndexType Index);
  bool empty() const { return IndexLookup.empty(); }

 private:
  // Cache that holds the set of uniquified symbols.
  std::shared_ptr<SymbolTable> Symtab;
  SymbolLookupType SymbolLookup;
  IndexLookupType IndexLookup;
  void installSymbols(const Node* Nd);
  void addSymbol(Symbol* Sym);
  void clear();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_CASM_SYMBOLINDEX_H
