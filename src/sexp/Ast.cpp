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

#include <unordered_map>

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
  {NodeType::BitToInt, "bit.to.int"},
  {NodeType::ByteToAst, "byte.to.ast"},
  {NodeType::ByteToBit, "byte.to.bit"},
  {NodeType::ByteToByte, "byte.to.byte"},
  {NodeType::ByteToInt, "byte.to.int"},
  {NodeType::Call, "call"},
  {NodeType::Case, "case"},
  {NodeType::Copy, "copy"},
  {NodeType::Default, "default"},
  {NodeType::Define, "define"},
  {NodeType::Error, "error"},
  {NodeType::Eval, "eval"},
  {NodeType::Extract, "extract"},
  {NodeType::ExtractBegin, "extract.begin"},
  {NodeType::ExtractEnd, "extract.end"},
  {NodeType::ExtractEof, "extract.eof"},
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
  {NodeType::Method, "method"},
  {NodeType::Peek, "peek"},
  {NodeType::Postorder, "preorder"},
  {NodeType::Preorder, "postorder" },
  {NodeType::Read, "read"},
  {NodeType::Section, "section"},
  {NodeType::Select, "select"},
  {NodeType::Sequence, "sequence"},
  {NodeType::Symbol, "symbol"},
  {NodeType::SymConst, "sym.const"},
  {NodeType::Uint32NoArgs, "uint32"},
  {NodeType::Uint32OneArg, "uint32"},
  {NodeType::Uint8, "uint8"},
  {NodeType::Uint64NoArgs, "uint64"},
  {NodeType::Uint64OneArg, "uint64"},
  {NodeType::U32Const, "u32.const"},
  {NodeType::U64Const, "u64.const"},
  {NodeType::Value, "value"},
  {NodeType::Varint32NoArgs, "varint32"},
  {NodeType::Varint32OneArg, "varint32"},
  {NodeType::Varint64NoArgs, "varint64"},
  {NodeType::Varint64OneArg, "varint64"},
  {NodeType::Varuint1, "varuint1"},
  {NodeType::Varuint7, "varuint7"},
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
  {NodeType::AppendNoArgs, "appendnoArgs"},
  {NodeType::AppendOneArg, "appendoneArg"},
  {NodeType::Uint32NoArgs, "uint32noArgs"},
  {NodeType::Uint32OneArg, "uint32oneArg"},
  {NodeType::Uint64NoArgs, "uint64noArgs"},
  {NodeType::Uint64OneArg, "uint64OneArg"},
  {NodeType::Varint32NoArgs, "varint32noArgs"},
  {NodeType::Varint32OneArg, "varint32oneArg"},
  {NodeType::Varint64NoArgs, "varint64noArgs"},
  {NodeType::Varint64OneArg, "varint64oneArg"},
  {NodeType::Varuint32NoArgs, "varuint32noArgs"},
  {NodeType::Varuint32OneArg, "varuint32oneArg"},
  {NodeType::Varuint64NoArgs, "varuint64noArgs"},
  {NodeType::Varuint64OneArg, "varuint64oneArg"},
};

} // end of anonymous namespace

namespace wasm {
namespace filt {

const char *getNodeSexpName(NodeType Type) {
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < size(SexpTypeNamePair); ++i) {
      const char *Name = SexpTypeNamePair[i].Name;
      Mapping[static_cast<int>(SexpTypeNamePair[i].Type)] = Name;
    }
  }
  const char *Name = Mapping[static_cast<int>(Type)];
  if (Name == nullptr)
    Mapping[static_cast<int>(Type)] = Name = "?NodeType?";
  return Name;
}

const char *getNodeTypeName(NodeType Type) {
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < size(UniquifyingTypeNamePair); ++i) {
      const char *Name = UniquifyingTypeNamePair[i].Name;
      Mapping[static_cast<int>(UniquifyingTypeNamePair[i].Type)] = Name;
    }
  }
  const char *Name = Mapping[static_cast<int>(Type)];
  if (Name != nullptr)
    return Name;
  return getNodeSexpName(Type);
}

NodeMemory NodeMemory::Default;

void Node::append(Node *) {
  decode::fatal("Node::append not supported for ast node!");
}

void NaryNode::append(Node *Kid) {
  Kids.push_back(Kid);
}

// Note: we create duumy virtual forceCompilation() to force legal
// class definitions to be compiled in here. Classes not explicitly
// instantiated here will not link. This used to force an error if
// a node type is not defined with the correct template class.

void IntegerNode::forceCompilation() {}

void SymbolNode::forceCompilation() {}

void IfThenElse::forceCompilation() {}

template<NodeType Kind>
void Nullary<Kind>::forceCompilation() {}

template<NodeType Kind>
void Unary<Kind>::forceCompilation() {}


template<NodeType Kind>
void Binary<Kind>::forceCompilation() {}

template<NodeType Kind>
void Nary<Kind>::forceCompilation() {}

template class Nullary<NodeType::AppendNoArgs>;
template class Nullary<NodeType::Copy>;
template class Nullary<NodeType::Error>;
template class Nullary<NodeType::ExtractEof>;
template class Nullary<NodeType::Uint8>;
template class Nullary<NodeType::Uint32NoArgs>;
template class Nullary<NodeType::Uint64NoArgs>;
template class Nullary<NodeType::Value>;
template class Nullary<NodeType::Varint32NoArgs>;
template class Nullary<NodeType::Varint64NoArgs>;
template class Nullary<NodeType::Varuint1>;
template class Nullary<NodeType::Varuint7>;
template class Nullary<NodeType::Varuint32NoArgs>;
template class Nullary<NodeType::Varuint64NoArgs>;
template class Nullary<NodeType::Void>;

template class Unary<NodeType::AppendOneArg>;
template class Unary<NodeType::AstToAst>;
template class Unary<NodeType::AstToBit>;
template class Unary<NodeType::AstToByte>;
template class Unary<NodeType::AstToInt>;
template class Unary<NodeType::BitToAst>;
template class Unary<NodeType::BitToBit>;
template class Unary<NodeType::BitToByte>;
template class Unary<NodeType::BitToInt>;
template class Unary<NodeType::ByteToAst>;
template class Unary<NodeType::ByteToBit>;
template class Unary<NodeType::ByteToByte>;
template class Unary<NodeType::ByteToInt>;
template class Unary<NodeType::ExtractBegin>;
template class Unary<NodeType::ExtractEnd>;
template class Unary<NodeType::Call>;
template class Unary<NodeType::Eval>;
template class Unary<NodeType::IntToAst>;
template class Unary<NodeType::IntToBit>;
template class Unary<NodeType::IntToByte>;
template class Unary<NodeType::IntToInt>;
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
template class Unary<NodeType::U32Const>;
template class Unary<NodeType::U64Const>;
template class Unary<NodeType::Varint32OneArg>;
template class Unary<NodeType::Varint64OneArg>;
template class Unary<NodeType::Varuint32OneArg>;
template class Unary<NodeType::Varuint64OneArg>;
template class Unary<NodeType::Version>;
template class Unary<NodeType::Write>;

template class Binary<NodeType::Map>;

template class Nary<NodeType::Case>;
template class Nary<NodeType::Extract>;
template class Nary<NodeType::Default>;
template class Nary<NodeType::Define>;
template class Nary<NodeType::File>;
template class Nary<NodeType::Filter>;
template class Nary<NodeType::Loop>;
template class Nary<NodeType::LoopUnbounded>;
template class Nary<NodeType::Method>;
template class Nary<NodeType::Section>;
template class Nary<NodeType::Select>;
template class Nary<NodeType::Sequence>;

}}
