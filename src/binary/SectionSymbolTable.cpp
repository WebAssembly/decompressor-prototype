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

// Implements the section symbol table that holds the set of symbols defined
// in a section.

#include "binary/SectionSymbolTable.h"

using namespace wasm::decode;

namespace wasm {

namespace filt {

void SectionSymbolTable::addSymbol(std::string& Name) {
  SymbolNode* Sym = Symtab.getSymbolDefinition(Name);
  if (SymbolLookup.count(Sym) == 0) {
    IndexType Index = IndexLookup.size();
    SymbolLookup[Sym] = Index;
    IndexLookup.push_back(Sym);
  }
}

void SectionSymbolTable::installSymbols(const Node* Nd) {
  if (const SymbolNode* Symbol = dyn_cast<SymbolNode>(Nd)) {
    std::string SymName = Symbol->getStringName();
    addSymbol(SymName);
  }
  for (const auto* Kid : *Nd)
    installSymbols(Kid);
}

void SectionSymbolTable::installSection(const SectionNode* Section) {
  // Install all kids but the first (i.e. the section name), since section
  // names must be explicitly defined.
  for (size_t i = 1, len = Section->getNumKids(); i < len; ++i)
    installSymbols(Section->getKid(i));
}

uint32_t SectionSymbolTable::getSymbolIndex(SymbolNode* Symbol) {
  std::string SymName = Symbol->getStringName();
  SymbolNode* Sym = Symtab.getSymbolDefinition(SymName);
  const auto Iter = SymbolLookup.find(Sym);
  if (Iter == SymbolLookup.end())
    fatal("Can't find index for symbol: " + Sym->getStringName());
  return Iter->second;
}

SymbolNode* SectionSymbolTable::getIndexSymbol(IndexType Index) {
  if (Index >= IndexLookup.size())
    fatal("Can't find symbol for index: " + std::to_string(Index));
  return IndexLookup[Index];
}

}  // end of namespace filt

}  // end of namespace wasm
