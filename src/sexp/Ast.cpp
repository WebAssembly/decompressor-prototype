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

/* Implements AST's for modeling filter s-expressions */

#include "Defs.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"

#include <cstring>
#include <unordered_map>

namespace wasm {

using namespace alloc;
using namespace decode;

namespace filt {

AstTraitsType AstTraits[NumNodeTypes] = {
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
  { Op##tag, sexp_name, type_name, text_num_args, text_max_args },
  AST_OPCODE_TABLE
#undef X
};

const char *getNodeSexpName(NodeType Type) {
  // TODO(KarlSchimpf): Make thread safe
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < NumNodeTypes; ++i) {
      AstTraitsType &Traits = AstTraits[i];
      Mapping[int(Traits.Type)] = Traits.SexpName;
    }
  }
  char *Name = const_cast<char*>(Mapping[static_cast<int>(Type)]);
  if (Name == nullptr) {
    std::string NewName(
        std::string("NodeType::") + std::to_string(static_cast<int>(Type)));
    Name = new char[NewName.size() + 1];
    Name[NewName.size()] ='\0';
    memcpy(Name, NewName.data(), NewName.size());
    Mapping[static_cast<int>(Type)] = Name;
  }
  return Name;
}

const char *getNodeTypeName(NodeType Type) {
  // TODO(KarlSchimpf): Make thread safe
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < NumNodeTypes; ++i) {
      AstTraitsType &Traits = AstTraits[i];
      if (Traits.TypeName)
        Mapping[int(Traits.Type)] = Traits.TypeName;
    }
  }
  const char *Name = Mapping[static_cast<int>(Type)];
  if (Name == nullptr)
    Mapping[static_cast<int>(Type)] =  Name = getNodeSexpName(Type);
  return Name;
}

void Node::forceCompilation() {}

void Node::append(Node *) {
  decode::fatal("Node::append not supported for ast node!");
}

// Note: we create duumy virtual forceCompilation() to force legal
// class definitions to be compiled in here. Classes not explicitly
// instantiated here will not link. This used to force an error if
// a node type is not defined with the correct template class.

void IntegerNode::forceCompilation() {}

void SymbolNode::forceCompilation() {}

std::string SymbolNode::getStringName() const {
  std::string Str(Name.size(), '\0');
  for (size_t i = 0; i < Name.size(); ++i)
    Str[i] = char(Name[i]);
  return Str;
}

void SymbolNode::setDefineDefinition(Node *Defn) {
  if (Defn) {
    IsDefineUsingDefault = false;
    DefineDefinition = Defn;
  } else {
    IsDefineUsingDefault = true;
    DefineDefinition = DefaultDefinition;
  }
}

void SymbolNode::setDefaultDefinition(Node *Defn) {
  assert(Defn != nullptr);
  DefaultDefinition = Defn;
  if (IsDefineUsingDefault) {
    DefineDefinition = Defn;
  }
}

SymbolTable::SymbolTable(alloc::Allocator *Alloc) : Alloc(Alloc) {
  Error = Alloc->create<ErrorNode>();
}

SymbolNode *SymbolTable::getSymbolDefinition(ExternalName &Name) {
  SymbolNode *Node = SymbolMap[Name];
  if (Node == nullptr) {
    Node = Alloc->create<SymbolNode>(Alloc, Name);
    Node->setDefaultDefinition(Error);
    SymbolMap[Name] = Node;
  }
  return Node;
}

void SymbolTable::install(Node *Root) {
  if (Root == nullptr)
    return;
  switch (Root->getType()) {
    default:
      return;
    case OpFile:
    case OpSection:
      for (Node *Kid : *Root)
        install(Kid);
      return;
    case OpDefine: {
      auto *DefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0));
      assert(DefineSymbol);
      DefineSymbol->setDefineDefinition(Root->getKid(1));
      return;
    }
    case OpDefault: {
      auto *DefaultSymbol = dyn_cast<SymbolNode>(Root->getKid(0));
      assert(DefaultSymbol);
      DefaultSymbol->setDefaultDefinition(Root->getKid(1));
      return;
    }
    case OpUndefine: {
      auto *UndefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0));
      assert(UndefineSymbol);
      UndefineSymbol->setDefineDefinition(nullptr);
    }
  }
}

Node *NullaryNode::getKid(int) const {
  return nullptr;
}

void NullaryNode::setKid(int, Node *) {
  decode::fatal("NullaryNode::setKid not allowed");
}

bool NullaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_NULLARYNODE_TABLE
#undef X
  }
}

#define X(tag)                                                                 \
  void tag##Node::forceCompilation() {}
  AST_NULLARYNODE_TABLE
#undef X

Node *UnaryNode::getKid(int Index) const {
  if (Index < 1)
    return Kids[0];
  return nullptr;
}

void UnaryNode::setKid(int Index, Node *NewValue) {
  assert(Index < 1);
  Kids[0] = NewValue;
}

bool UnaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_UNARYNODE_TABLE
#undef X
  }
}

#define X(tag)                                                                 \
  void tag##Node::forceCompilation() {}
  AST_UNARYNODE_TABLE
#undef X

Node *BinaryNode::getKid(int Index) const {
  if (Index < 2)
    return Kids[Index];
  return nullptr;
}

void BinaryNode::setKid(int Index, Node *NewValue) {
  assert(Index < 2);
  Kids[Index] = NewValue;
}

bool BinaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_BINARYNODE_TABLE
#undef X
  }
}

#define X(tag)                                                                 \
  void tag##Node::forceCompilation() {}
  AST_BINARYNODE_TABLE
#undef X

Node *TernaryNode::getKid(int Index) const {
  if (Index < 3)
    return Kids[Index];
  return nullptr;
}

void TernaryNode::setKid(int Index, Node *NewValue) {
  assert(Index < 3);
  Kids[Index] = NewValue;
}

bool TernaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_TERNARYNODE_TABLE
#undef X
  }
}

#define X(tag)                                                                 \
  void tag##Node::forceCompilation() {}
  AST_TERNARYNODE_TABLE
#undef X

bool NaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_NARYNODE_TABLE
#undef X
  }
}

void NaryNode::forceCompilation() {}

void SelectNode::forceCompilation() {}

const Node *SelectNode::getCase(IntType Key) const {
  if (LookupMap.count(Key))
    return LookupMap.at(Key);
  return nullptr;
}

void SelectNode::installFastLookup() {
  TextWriter Writer;
  for (auto *Kid : *this) {
    if (const auto *Case = dyn_cast<CaseNode>(Kid)) {
      if (const auto *Key = dyn_cast<IntegerNode>(Case->getKid(0))) {
        LookupMap[Key->getValue()] = Kid;
      }
    }
  }
}

#define X(tag)                                                                 \
  void tag##Node::forceCompilation() {}
AST_NARYNODE_TABLE;
#undef X

void SectionSymbolTable::installSymbols(const Node *Nd) {
  if (const SymbolNode *Symbol = dyn_cast<SymbolNode>(Nd)) {
    std::string SymName = Symbol->getStringName();
    SymbolNode *Sym = Symtab.getSymbolDefinition(SymName);
    if (SymbolMap.count(Sym) == 0) {
      SymbolMap.emplace(Sym, SymbolMap.size());
    }
  }
  for (const auto *Kid : *Nd)
    installSymbols(Kid);
}

void SectionSymbolTable::installSection(const SectionNode *Section) {
  // Install all kids but the first (i.e. the section name), since section
  // names must be explicitly defined.
  for (size_t i = 1, len = Section->getNumKids(); i < len; ++i)
    installSymbols(Section->getKid(i));
  std::unordered_map<uint32_t, const SymbolNode *> InverseMap;
  for (const auto &Pair : SymbolMap)
    InverseMap[Pair.second] = Pair.first;
  for (size_t i = 0, len = SymbolMap.size(); i < len; ++i)
    SymbolVector.push_back(InverseMap[i]);
}

uint32_t SectionSymbolTable::getStringIndex(const SymbolNode *Symbol) {
  std::string SymName = Symbol->getStringName();
  SymbolNode *Sym = Symtab.getSymbolDefinition(SymName);
  const auto Iter = SymbolMap.find(Sym);
  if (Iter == SymbolMap.end())
    fatal("Can't find string index for: " + Sym->getStringName());
  return Iter->second;
}

} // end of namespace filt

} // end of namespace wasm
