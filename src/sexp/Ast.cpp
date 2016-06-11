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

namespace wasm {
namespace filt {

NodeMemory NodeMemory::Default;

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

template class Nullary<NodeType::Append>;
template class Nullary<NodeType::Copy>;
template class Nullary<NodeType::Uint8>;
template class Nullary<NodeType::Uint32>;
template class Nullary<NodeType::Uint64>;
template class Nullary<NodeType::Value>;
template class Nullary<NodeType::Varint32>;
template class Nullary<NodeType::Varint64>;
template class Nullary<NodeType::Varuint1>;
template class Nullary<NodeType::Varuint7>;
template class Nullary<NodeType::Varuint32>;
template class Nullary<NodeType::Varuint64>;
template class Nullary<NodeType::Void>;

template class Unary<NodeType::AppendValue>;
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
template class Unary<NodeType::Call>;
template class Unary<NodeType::Eval>;
template class Unary<NodeType::Fixed32>;
template class Unary<NodeType::Fixed64>;
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
template class Unary<NodeType::U32Const>;
template class Unary<NodeType::U64Const>;
template class Unary<NodeType::Vbrint32>;
template class Unary<NodeType::Vbrint64>;
template class Unary<NodeType::Vbruint32>;
template class Unary<NodeType::Vbruint64>;
template class Unary<NodeType::Version>;
template class Unary<NodeType::Write>;

template class Binary<NodeType::Map>;

template class Nary<NodeType::Case>;
template class Nary<NodeType::Define>;
template class Nary<NodeType::Extract>;
template class Nary<NodeType::File>;
template class Nary<NodeType::Filter>;
template class Nary<NodeType::Loop>;
template class Nary<NodeType::LoopUnbounded>;
template class Nary<NodeType::Method>;
template class Nary<NodeType::Section>;
template class Nary<NodeType::Select>;
template class Nary<NodeType::Sequence>;

}}
