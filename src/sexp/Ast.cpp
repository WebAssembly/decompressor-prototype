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

#include <iostream>


namespace wasm {

using namespace alloc;

namespace filt {

AstTraitsType AstTraits[NumNodeTypes] = {
#define X(tag, opcode, sexp_name, type_name, TextNumArgs) \
  { Op##tag, sexp_name, type_name, TextNumArgs },
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


template<NodeType Kind>
void Nullary<Kind>::forceCompilation() {}

template<NodeType Kind>
void Unary<Kind>::forceCompilation() {}


template<NodeType Kind>
void Binary<Kind>::forceCompilation() {}

template<NodeType Kind>
void Ternary<Kind>::forceCompilation() {}

void NaryNode::forceCompilation() {}

template<NodeType Kind>
void Nary<Kind>::forceCompilation() {}

template class Nullary<OpAppendNoArgs>;
template class Nullary<OpBlockBegin>;
template class Nullary<OpBlockEnd>;
template class Nullary<OpCopy>;
template class Nullary<OpError>;
template class Nullary<OpUint8NoArgs>;
template class Nullary<OpUint32NoArgs>;
template class Nullary<OpUint64NoArgs>;
template class Nullary<OpVarint32NoArgs>;
template class Nullary<OpVarint64NoArgs>;
template class Nullary<OpVaruint1NoArgs>;
template class Nullary<OpVaruint7NoArgs>;
template class Nullary<OpVaruint32NoArgs>;
template class Nullary<OpVaruint64NoArgs>;
template class Nullary<OpVoid>;

template class Unary<OpAppendOneArg>;
template class Unary<OpBlockOneArg>;
template class Unary<OpEval>;
template class Unary<OpI32Const>;
template class Unary<OpI64Const>;
template class Unary<OpLit>;
template class Unary<OpPeek>;
template class Unary<OpPostorder>;
template class Unary<OpPreorder>;
template class Unary<OpRead>;
template class Unary<OpSymConst>;
template class Unary<OpUint32OneArg>;
template class Unary<OpUint64OneArg>;
template class Unary<OpUndefine>;
template class Unary<OpU32Const>;
template class Unary<OpU64Const>;
template class Unary<OpUint8OneArg>;
template class Unary<OpVarint32OneArg>;
template class Unary<OpVarint64OneArg>;
template class Unary<OpVaruint1OneArg>;
template class Unary<OpVaruint7OneArg>;
template class Unary<OpVaruint32OneArg>;
template class Unary<OpVaruint64OneArg>;
template class Unary<OpVersion>;
template class Unary<OpWrite>;

template class Binary<OpBlockTwoArgs>;
template class Binary<OpMap>;

template class Ternary<OpIfThenElse>;
template class Ternary<OpBlockThreeArgs>;

template class Nary<OpAstToAst>;
template class Nary<OpAstToBit>;
template class Nary<OpAstToByte>;
template class Nary<OpAstToInt>;
template class Nary<OpBitToAst>;
template class Nary<OpBitToBit>;
template class Nary<OpBitToByte>;
template class Nary<OpBitToInt>;
template class Nary<OpByteToAst>;
template class Nary<OpByteToBit>;
template class Nary<OpByteToByte>;
template class Nary<OpByteToInt>;
template class Nary<OpCase>;
template class Nary<OpDefault>;
template class Nary<OpDefine>;
template class Nary<OpFile>;
template class Nary<OpFilter>;
template class Nary<OpIntToAst>;
template class Nary<OpIntToBit>;
template class Nary<OpIntToByte>;
template class Nary<OpIntToInt>;
template class Nary<OpLoop>;
template class Nary<OpLoopUnbounded>;
template class Nary<OpSection>;
template class Nary<OpSelect>;
template class Nary<OpSequence>;

}}
