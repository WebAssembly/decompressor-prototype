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

#ifndef DECOMPRESSOR_SRC_BINARY_SECTIONSYMBOLTABLE_H
#define DECOMPRESSOR_SRC_BINARY_SECTIONSYMBOLTABLE_H

#include "sexp/Ast.h"
#include "utils/Defs.h"

#include <unordered_map>
#include <vector>

namespace wasm {

namespace filt {

class SectionSymbolTable {
  SectionSymbolTable(const SectionSymbolTable&) = delete;
  SectionSymbolTable& operator=(const SectionSymbolTable&) = delete;

 public:
  using IndexType = uint32_t;
  using SymbolLookupType = std::unordered_map<SymbolNode*, IndexType>;
  using IndexLookupType = std::vector<SymbolNode*>;
  SectionSymbolTable(alloc::Allocator* Alloc) : Symtab(Alloc) {}
  ~SectionSymbolTable() {}
  void installSection(const SectionNode* Section);
  const IndexLookupType& getVector() { return IndexLookup; }
  void addSymbol(std::string& Name);
  uint32_t getSymbolIndex(SymbolNode* Symbol);
  void clear() {
    Symtab.clear();
    SymbolLookup.clear();
    IndexLookup.clear();
  }
  SymbolNode* getIndexSymbol(IndexType Index);
  bool empty() const { return IndexLookup.empty(); }

 private:
  // Cache that holds the set of uniquified symbols.
  SymbolTable Symtab;
  SymbolLookupType SymbolLookup;
  IndexLookupType IndexLookup;
  void installSymbols(const Node* Nd);
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_SECTIONSYMBOLTABLE_H
