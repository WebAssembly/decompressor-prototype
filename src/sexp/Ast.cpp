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

namespace {

using namespace wasm::filt;

typedef struct { NodeType Type; const char* Name; } TypeNamePair;

TypeNamePair SexpTypeNamePair[] = {
  {NodeType::AppendNoArgs, "append" },
  {NodeType::AppendOneArg, "append" },
  {NodeType::AstToAst, "ast.to.ast"},
  {NodeType::AstToBit, "ast.to.bit"},
  {NodeType::AstToByte, "ast.to.byte"},
  {NodeType::AstToInt, "ast.to.int"},
  {NodeType::BitToAst, "bit.to.ast"},
  {NodeType::BitToBit, "bit.to.bit"},
  {NodeType::BitToByte, "bit.to.byte"},
  {NodeType::BlockBegin, "block.begin"},
  {NodeType::BlockEnd, "block.end"},
  {NodeType::BlockOneArg, "block"},
  {NodeType::BlockThreeArgs, "block"},
  {NodeType::BlockTwoArgs, "block"},
  {NodeType::BitToInt, "bit.to.int"},
  {NodeType::ByteToAst, "byte.to.ast"},
  {NodeType::ByteToBit, "byte.to.bit"},
  {NodeType::ByteToByte, "byte.to.byte"},
  {NodeType::ByteToInt, "byte.to.int"},
  {NodeType::Case, "case"},
  {NodeType::Copy, "copy"},
  {NodeType::Default, "default"},
  {NodeType::Define, "define"},
  {NodeType::Error, "error"},
  {NodeType::Eval, "eval"},
  {NodeType::File, "file"},
  {NodeType::Filter, "filter"},
  {NodeType::IfThenElse, "if"},
  {NodeType::Integer, "<integer>"},
  {NodeType::IntToAst, "int.to.ast"},
  {NodeType::IntToBit, "int.to.bit"},
  {NodeType::IntToByte, "int.to.byte"},
  {NodeType::IntToInt, "int.to.int"},
  {NodeType::I32Const, "i32.const"},
  {NodeType::I64Const, "i64.const"},
  {NodeType::Lit, "lit"},
  {NodeType::Loop, "loop"},
  {NodeType::LoopUnbounded, "loop.unbounded"},
  {NodeType::Map, "map"},
  {NodeType::Peek, "peek"},
  {NodeType::Postorder, "postorder"},
  {NodeType::Preorder, "preorder" },
  {NodeType::Read, "read"},
  {NodeType::Section, "section"},
  {NodeType::Select, "select"},
  {NodeType::Sequence, "seq"},
  {NodeType::Symbol, "symbol"},
  {NodeType::SymConst, "sym.const"},
  {NodeType::Uint32NoArgs, "uint32"},
  {NodeType::Uint32OneArg, "uint32"},
  {NodeType::Uint8NoArgs, "uint8"},
  {NodeType::Uint8OneArg, "uint8"},
  {NodeType::Uint64NoArgs, "uint64"},
  {NodeType::Uint64OneArg, "uint64"},
  {NodeType::Undefine, "undefine"},
  {NodeType::U32Const, "u32.const"},
  {NodeType::U64Const, "u64.const"},
  {NodeType::Varint32NoArgs, "varint32"},
  {NodeType::Varint32OneArg, "varint32"},
  {NodeType::Varint64NoArgs, "varint64"},
  {NodeType::Varint64OneArg, "varint64"},
  {NodeType::Varuint1NoArgs, "varuint1"},
  {NodeType::Varuint1OneArg, "varuint1"},
  {NodeType::Varuint7NoArgs, "varuint7"},
  {NodeType::Varuint7OneArg, "varuint7"},
  {NodeType::Varuint32NoArgs, "varuint32"},
  {NodeType::Varuint32OneArg, "varuint32"},
  {NodeType::Varuint64NoArgs, "varuint64"},
  {NodeType::Varuint64OneArg, "varuint64"},
  {NodeType::Version, "version"},
  {NodeType::Void, "void"},
  {NodeType::Write, "write"}
};

// Defines clarifying unique names if SexpTypeNamePair not unique.
TypeNamePair UniquifyingTypeNamePair[] = {
  {NodeType::AppendNoArgs, "appendNoArgs"},
  {NodeType::AppendOneArg, "appendOneArg"},
  {NodeType::BlockOneArg, "blockOneArg"},
  {NodeType::BlockThreeArgs, "blockThreeArgs"},
  {NodeType::BlockTwoArgs, "blockTwoArgs"},
  {NodeType::Uint8NoArgs, "uint8NoArgs"},
  {NodeType::Uint8OneArg, "uint8OneArg"},
  {NodeType::Uint32NoArgs, "uint32NoArgs"},
  {NodeType::Uint32OneArg, "uint32OneArg"},
  {NodeType::Uint64NoArgs, "uint64NoArgs"},
  {NodeType::Uint64OneArg, "uint64OneArg"},
  {NodeType::Varint32NoArgs, "varint32NoArgs"},
  {NodeType::Varint32OneArg, "varint32OneArg"},
  {NodeType::Varint64NoArgs, "varint64NoArgs"},
  {NodeType::Varint64OneArg, "varint64OneArg"},
  {NodeType::Varuint1NoArgs, "varuint1NoArgs"},
  {NodeType::Varuint1OneArg, "varuint1OneArg"},
  {NodeType::Varuint7NoArgs, "varuint7NoArgs"},
  {NodeType::Varuint7OneArg, "varuint7OneArg"},
  {NodeType::Varuint32NoArgs, "varuint32NoArgs"},
  {NodeType::Varuint32OneArg, "varuint32OneArg"},
  {NodeType::Varuint64NoArgs, "varuint64NoArgs"},
  {NodeType::Varuint64OneArg, "varuint64OneArg"},
};

} // end of anonymous namespace

using namespace wasm::alloc;

namespace wasm {
namespace filt {

const char *getNodeSexpName(NodeType Type) {
  // TODO(KarlSchimpf): Make thread safe
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < size(SexpTypeNamePair); ++i) {
      const char *Name = SexpTypeNamePair[i].Name;
      Mapping[static_cast<int>(SexpTypeNamePair[i].Type)] = Name;
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
    for (size_t i = 0; i < size(UniquifyingTypeNamePair); ++i) {
      const char *Name = UniquifyingTypeNamePair[i].Name;
      Mapping[static_cast<int>(UniquifyingTypeNamePair[i].Type)] = Name;
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

template class Nullary<NodeType::AppendNoArgs>;
template class Nullary<NodeType::BlockBegin>;
template class Nullary<NodeType::BlockEnd>;
template class Nullary<NodeType::Copy>;
template class Nullary<NodeType::Error>;
template class Nullary<NodeType::Uint8NoArgs>;
template class Nullary<NodeType::Uint32NoArgs>;
template class Nullary<NodeType::Uint64NoArgs>;
template class Nullary<NodeType::Varint32NoArgs>;
template class Nullary<NodeType::Varint64NoArgs>;
template class Nullary<NodeType::Varuint1NoArgs>;
template class Nullary<NodeType::Varuint7NoArgs>;
template class Nullary<NodeType::Varuint32NoArgs>;
template class Nullary<NodeType::Varuint64NoArgs>;
template class Nullary<NodeType::Void>;

template class Unary<NodeType::AppendOneArg>;
template class Unary<NodeType::BlockOneArg>;
template class Unary<NodeType::Eval>;
template class Unary<NodeType::I32Const>;
template class Unary<NodeType::I64Const>;
template class Unary<NodeType::Lit>;
template class Unary<NodeType::Peek>;
template class Unary<NodeType::Postorder>;
template class Unary<NodeType::Preorder>;
template class Unary<NodeType::Read>;
template class Unary<NodeType::SymConst>;
template class Unary<NodeType::Uint32OneArg>;
template class Unary<NodeType::Uint64OneArg>;
template class Unary<NodeType::Undefine>;
template class Unary<NodeType::U32Const>;
template class Unary<NodeType::U64Const>;
template class Unary<NodeType::Uint8OneArg>;
template class Unary<NodeType::Varint32OneArg>;
template class Unary<NodeType::Varint64OneArg>;
template class Unary<NodeType::Varuint1OneArg>;
template class Unary<NodeType::Varuint7OneArg>;
template class Unary<NodeType::Varuint32OneArg>;
template class Unary<NodeType::Varuint64OneArg>;
template class Unary<NodeType::Version>;
template class Unary<NodeType::Write>;

template class Binary<NodeType::BlockTwoArgs>;
template class Binary<NodeType::Map>;

template class Ternary<NodeType::IfThenElse>;
template class Ternary<NodeType::BlockThreeArgs>;

template class Nary<NodeType::AstToAst>;
template class Nary<NodeType::AstToBit>;
template class Nary<NodeType::AstToByte>;
template class Nary<NodeType::AstToInt>;
template class Nary<NodeType::BitToAst>;
template class Nary<NodeType::BitToBit>;
template class Nary<NodeType::BitToByte>;
template class Nary<NodeType::BitToInt>;
template class Nary<NodeType::ByteToAst>;
template class Nary<NodeType::ByteToBit>;
template class Nary<NodeType::ByteToByte>;
template class Nary<NodeType::ByteToInt>;
template class Nary<NodeType::Case>;
template class Nary<NodeType::Default>;
template class Nary<NodeType::Define>;
template class Nary<NodeType::File>;
template class Nary<NodeType::Filter>;
template class Nary<NodeType::IntToAst>;
template class Nary<NodeType::IntToBit>;
template class Nary<NodeType::IntToByte>;
template class Nary<NodeType::IntToInt>;
template class Nary<NodeType::Loop>;
template class Nary<NodeType::LoopUnbounded>;
template class Nary<NodeType::Section>;
template class Nary<NodeType::Select>;
template class Nary<NodeType::Sequence>;

}}
