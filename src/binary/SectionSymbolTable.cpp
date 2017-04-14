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

#include "sexp/Ast.h"
#include "utils/Casting.h"

namespace wasm {

using namespace decode;

namespace filt {

SectionSymbolTable::SectionSymbolTable(std::shared_ptr<SymbolTable> Symtab)
    : Symtab(Symtab) {
}

void SectionSymbolTable::addSymbol(const std::string& Name) {
  addSymbol(Symtab->getOrCreateSymbol(Name));
}

void SectionSymbolTable::addSymbol(Symbol* Sym) {
  if (Sym->getPredefinedSymbol() != PredefinedSymbol::Unknown)
    return;
  if (SymbolLookup.count(Sym) == 0) {
    IndexType Index = IndexLookup.size();
    SymbolLookup[Sym] = Index;
    IndexLookup.push_back(Sym);
  }
}

void SectionSymbolTable::clear() {
  Symtab->clearSymbols();
  SymbolLookup.clear();
  IndexLookup.clear();
}

void SectionSymbolTable::install(File* Root) {
  Symtab->setRoot(Root);
  Symtab->install();
}

void SectionSymbolTable::installSymbols(const Node* Nd) {
  // TODO(karlschimpf) Make this non-recursive.
  if (const Symbol* Sym = dyn_cast<Symbol>(Nd)) {
    addSymbol(Sym->getName());
  }
  for (const auto* Kid : *Nd)
    installSymbols(Kid);
}

void SectionSymbolTable::installSection(const Section* Sec) {
  for (size_t i = 0, len = Sec->getNumKids(); i < len; ++i)
    installSymbols(Sec->getKid(i));
}

uint32_t SectionSymbolTable::getSymbolIndex(Symbol* ForSym) {
  PredefinedSymbol Sym = ForSym->getPredefinedSymbol();
  if (Sym != PredefinedSymbol::Unknown)
    return uint32_t(Sym);
  const auto Iter = SymbolLookup.find(ForSym);
  if (Iter == SymbolLookup.end())
    fatal("Can't find index for symbol: " + ForSym->getName());
  return Iter->second + NumPredefinedSymbols;
}

Symbol* SectionSymbolTable::getIndexSymbol(IndexType Index) {
  if (Index < NumPredefinedSymbols)
    return Symtab->getPredefined(PredefinedSymbol(Index));
  Index -= NumPredefinedSymbols;
  if (Index >= IndexLookup.size())
    fatal("Can't find symbol for index: " + std::to_string(Index));
  return IndexLookup[Index];
}

}  // end of namespace filt

}  // end of namespace wasm
