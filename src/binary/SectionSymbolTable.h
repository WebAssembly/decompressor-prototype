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
class Section;

class SectionSymbolTable {
  SectionSymbolTable(const SectionSymbolTable&) = delete;
  SectionSymbolTable& operator=(const SectionSymbolTable&) = delete;

 public:
  typedef uint32_t IndexType;
  typedef std::unordered_map<Symbol*, IndexType> SymbolLookupType;
  typedef std::vector<Symbol*> IndexLookupType;
  SectionSymbolTable(std::shared_ptr<SymbolTable> Symtab);
  ~SectionSymbolTable() {}
  void installSection(const Section* Sec);
  const IndexLookupType& getVector() { return IndexLookup; }
  void addSymbol(const std::string& Name);
  uint32_t getSymbolIndex(Symbol* Sym);
  IndexType getNumberSymbols() const { return IndexLookup.size(); }
  void clear();
  Symbol* getIndexSymbol(IndexType Index);
  bool empty() const { return IndexLookup.empty(); }
  void install(Algorithm* Root);

 private:
  // Cache that holds the set of uniquified symbols.
  std::shared_ptr<SymbolTable> Symtab;
  SymbolLookupType SymbolLookup;
  IndexLookupType IndexLookup;
  void installSymbols(const Node* Nd);
  void addSymbol(Symbol* Sym);
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_SECTIONSYMBOLTABLE_H
