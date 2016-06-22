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

#include <cstring>
#include <unordered_map>

namespace wasm {

using namespace alloc;

namespace filt {

AstTraitsType AstTraits[NumNodeTypes] = {
#define X(tag, opcode, sexp_name, type_name, text_num_args) \
  { Op##tag, sexp_name, type_name, text_num_args },
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

void Node::append(Node *) {
  decode::fatal("Node::append not supported for ast node!");
}

// Note: we create duumy virtual forceCompilation() to force legal
// class definitions to be compiled in here. Classes not explicitly
// instantiated here will not link. This used to force an error if
// a node type is not defined with the correct template class.

void IntegerNode::forceCompilation() {}

void SymbolNode::forceCompilation() {}

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
  Error = Alloc->create<Nullary<OpError>>();
}

SymbolNode *SymbolTable::getSymbol(ExternalName &Name) {
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

bool NullaryNode::inClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_NULLARYNODE_TABLE
#undef X
  }
}

template<NodeType Kind>
void Nullary<Kind>::forceCompilation() {}

bool UnaryNode::inClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_UNARYNODE_TABLE
#undef X
  }
}

template<NodeType Kind>
void Unary<Kind>::forceCompilation() {}

bool BinaryNode::inClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_BINARYNODE_TABLE
#undef X
  }
}

template<NodeType Kind>
void Binary<Kind>::forceCompilation() {}

bool TernaryNode::inClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_TERNARYNODE_TABLE
#undef X
  }
}

template<NodeType Kind>
void Ternary<Kind>::forceCompilation() {}

bool NaryNode::inClass(NodeType Type) {
  switch (Type) {
    default: return false;
#define X(tag) \
    case Op##tag: return true;
    AST_NARYNODE_TABLE
#undef X
  }
}

void NaryNode::forceCompilation() {}

template<NodeType Kind>
void Nary<Kind>::forceCompilation() {}

#define X(tag) \
  template class Nullary<Op##tag>;
  AST_NULLARYNODE_TABLE
#undef X

#define X(tag) \
  template class Unary<Op##tag>;
  AST_UNARYNODE_TABLE
#undef X

#define X(tag) \
  template class Binary<Op##tag>;
  AST_BINARYNODE_TABLE
#undef X

#define X(tag) \
  template class Ternary<Op##tag>;
  AST_TERNARYNODE_TABLE
#undef X

#define X(tag) \
  template class Nary<Op##tag>;
  AST_NARYNODE_TABLE
#undef X

}}
