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

// Defines an internal model of filter AST's.
//
// NOTE: Many classes define  virtual:
//
//    virtual void forceCompilation() final;
//
// This is done to force the compilation of virtuals associated with the
// class in file Ast.cpp.
//
// Note: Classes allow an optional allocator as the first argument. This
// allows the creator to decide what allocator will be used internally.

#ifndef DECOMPRESSOR_SRC_SEXP_AST_H
#define DECOMPRESSOR_SRC_SEXP_AST_H

#include "Defs.h"

#include "Allocator.h"
#include "ADT/arena_vector.h"
#include "Ast.def"

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace wasm {

namespace filt {

enum NodeType {
#define X(tag, opcode, sexp_name, type_name, TextNumArgs) Op##tag = opcode,
  AST_OPCODE_TABLE
#undef X
};

static constexpr size_t NumNodeTypes =
    0
#define X(tag, opcode, sexp_name, type_name, TextNumArgs) + 1
    AST_OPCODE_TABLE
#undef X
    ;

static constexpr size_t MaxNodeType =
    const_maximum(
#define X(tag, opcode, sexp_name, type_name, TextNumArgs) size_t(opcode),
  AST_OPCODE_TABLE
#undef X
  std::numeric_limits<size_t>::min());

struct AstTraitsType {
  const NodeType Type;
  const char *SexpName;
  const char *TypeName;
  const int NumTextArgs;
};

extern AstTraitsType AstTraits[NumNodeTypes];

// Returns the s-expression name
const char *getNodeSexpName(NodeType Type);

// Returns a unique (printable) type name
const char *getNodeTypeName(NodeType Type);

class Node {
  Node(const Node&) = delete;
  Node &operator=(const Node&) = delete;
  Node() = delete;
public:
  using IndexType = size_t;
  class Iterator {
  public:
    explicit Iterator(const Node *Node, IndexType Index)
        : Node(Node), Index(Index) {}
    Iterator(const Iterator &Iter): Node(Iter.Node), Index(Iter.Index) {}
    Iterator &operator=(const Iterator &Iter) {
      Node = Iter.Node;
      Index = Iter.Index;
      return *this;
    }
    void operator++() {
      ++Index;
    }
    void operator--() {
      --Index;
    }
    bool operator==(const Iterator &Iter) {
      return Node == Iter.Node && Index == Iter.Index;
    }
    bool operator!=(const Iterator &Iter) {
      return Node != Iter.Node || Index != Iter.Index;
    }
    Node *operator*() const {
      return Node->getKid(Index);
    }
  private:
    const Node *Node;
    IndexType Index;
  };

  virtual ~Node() {}
  NodeType getType() const {
    return Type;
  }
  IndexType getNumKids() const {
    return Kids.size();
  }
  Node *getKid(IndexType Index) const {
    return Kids[Index];
  }
  // WARNING: Only supported if underlying type allows.
  virtual void append(Node *Kid);
  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, getNumKids()); }
  Iterator rbegin() { return Iterator(this, getNumKids() - 1); }
  Iterator rend() const { return Iterator(this, -1); }
protected:
  NodeType Type;
  arena_vector<Node*> Kids;
  Node(NodeType Type)
      : Type(Type), Kids(alloc::Allocator::Default) {}
  Node(alloc::Allocator *Alloc, NodeType Type)
      : Type(Type), Kids(Alloc) {}
};

class NullaryNode : public Node {
  NullaryNode(const NullaryNode&) = delete;
  NullaryNode &operator=(const NullaryNode&) = delete;
public:
  ~NullaryNode() override {}
protected:
  NullaryNode(NodeType Type)
      : Node(alloc::Allocator::Default, Type) {}
  NullaryNode(alloc::Allocator *Alloc, NodeType Type)
      : Node(Alloc, Type) {}
};

template<NodeType Kind>
class Nullary final : public NullaryNode {
  Nullary(const Nullary<Kind>&) = delete;
  Nullary<Kind> &operator=(const Nullary<Kind>&) = delete;
  virtual void forceCompilation() final;
public:
  Nullary() : NullaryNode(Kind) {}
  Nullary(alloc::Allocator *Alloc) : NullaryNode(Alloc, Kind) {}
  ~Nullary() {}
};

class IntegerNode final : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode &operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;
  virtual void forceCompilation() final;
public:
  // Note: ValueFormat provided so that we can echo back out same
  // representation as when lexing s-expressions.
  enum ValueFormat { Decimal, SignedDecimal, Hexidecimal };
  IntegerNode(decode::IntType Value, ValueFormat Format = Decimal) :
      NullaryNode(OpInteger),
      Value(Value),
      Format(Format) {}
  IntegerNode(alloc::Allocator* Alloc, decode::IntType Value,
              ValueFormat Format = Decimal) :
      NullaryNode(Alloc, OpInteger),
      Value(Value),
      Format(Format) {}
  ~IntegerNode() {}
  ValueFormat getFormat() const {
    return Format;
  }
  decode::IntType getValue() const {
    return Value;
  }
private:
  decode::IntType Value;
  ValueFormat Format;
};

class SymbolNode final : public NullaryNode {
  SymbolNode(const SymbolNode&) = delete;
  SymbolNode &operator=(const SymbolNode&) = delete;
  SymbolNode() = delete;
  virtual void forceCompilation() final;
public:
  using NameType = std::vector<uint8_t>;
  using InternalNameType = arena_vector<uint8_t>;
  SymbolNode(NameType &_Name)
      : NullaryNode(OpSymbol), Name(alloc::Allocator::Default) {
    init(_Name);
  }
  SymbolNode(alloc::Allocator *Alloc, NameType &_Name)
      : NullaryNode(Alloc, OpSymbol), Name(Alloc) {
    init(_Name);
  }
  ~SymbolNode() {}
  const InternalNameType &getName() const {
    return Name;
  }
private:
  InternalNameType Name;
  void init(NameType &_Name) {
    Name.reserve(Name.size());
    for (const auto &V : _Name)
      Name.emplace_back(V);
  }
};

class UnaryNode : public Node {
  UnaryNode(const UnaryNode&) = delete;
  UnaryNode &operator=(const UnaryNode&) = delete;
public:
  ~UnaryNode() override {}
protected:
  UnaryNode(NodeType Type, Node *Kid)
      : Node(Type) {
    Kids.emplace_back(Kid);
  }
  UnaryNode(alloc::Allocator *Alloc, NodeType Type, Node *Kid)
      : Node(Alloc, Type) {
    Kids.emplace_back(Kid);
  }
};

template<NodeType Kind>
class Unary final : public UnaryNode {
  Unary(const Unary<Kind>&) = delete;
  Unary<Kind> &operator=(const Unary<Kind>&) = delete;
  virtual void forceCompilation() final;
public:
  Unary(Node *Kid) : UnaryNode(Kind, Kid) {}
  Unary(alloc::Allocator* Alloc, Node *Kid) : UnaryNode(Alloc, Kind, Kid) {}
  ~Unary() {}
};

class BinaryNode : public Node {
  BinaryNode(const BinaryNode&) = delete;
  BinaryNode &operator=(const BinaryNode&) = delete;
public:
  ~BinaryNode() override {}
protected:
  BinaryNode(NodeType Type, Node *Kid1, Node *Kid2)
      : Node(Type) {
    Kids.emplace_back(Kid1);
    Kids.emplace_back(Kid2);
  }
  BinaryNode(alloc::Allocator *Alloc, NodeType Type, Node *Kid1, Node *Kid2)
      : Node(Alloc, Type) {
    Kids.emplace_back(Kid1);
    Kids.emplace_back(Kid2);
  }
};

template<NodeType Kind>
class Binary final : public BinaryNode {
  Binary(const Binary<Kind>&) = delete;
  Binary<Kind> &operator=(const Binary<Kind>&) = delete;
  virtual void forceCompilation() final;
public:
  Binary(Node *Kid1, Node *Kid2) : BinaryNode(Kind, Kid1, Kid2) {}
  Binary(alloc::Allocator *Alloc, Node *Kid1, Node *Kid2)
      : BinaryNode(Alloc, Kind, Kid1, Kid2) {}
  ~Binary() {}
};

class TernaryNode : public Node {
  TernaryNode(const TernaryNode&) = delete;
  TernaryNode &operator=(const TernaryNode&) = delete;
public:
  ~TernaryNode() override {}
protected:
  TernaryNode(NodeType Type, Node *Kid1, Node *Kid2, Node *Kid3)
      : Node(Type) {
    Kids.emplace_back(Kid1);
    Kids.emplace_back(Kid2);
    Kids.emplace_back(Kid3);
  }
  TernaryNode(alloc::Allocator *Alloc, NodeType Type,
              Node *Kid1, Node *Kid2, Node *Kid3)
      : Node(Alloc, Type) {
    Kids.emplace_back(Kid1);
    Kids.emplace_back(Kid2);
    Kids.emplace_back(Kid3);
  }
};

template<NodeType Kind>
class Ternary final : public TernaryNode {
  Ternary(const Ternary<Kind>&) = delete;
  Ternary<Kind> &operator=(const Ternary<Kind>&) = delete;
  virtual void forceCompilation() final;
public:
  Ternary(Node *Kid1, Node *Kid2, Node* Kid3)
      : TernaryNode(Kind, Kid1, Kid2, Kid3) {}
  Ternary(alloc::Allocator *Alloc, Node *Kid1, Node *Kid2, Node* Kid3)
      : TernaryNode(Alloc, Kind, Kid1, Kid2, Kid3) {}
  ~Ternary() {}
};

class NaryNode : public Node {
  NaryNode(const NaryNode&) = delete;
  NaryNode &operator=(const NaryNode&) = delete;
  virtual void forceCompilation();
public:
  void append(Node *Kid) final {
    Kids.emplace_back(Kid);
  }
  ~NaryNode() override {}
protected:
  NaryNode(NodeType Type) : Node(Type) {}
  NaryNode(alloc::Allocator *Alloc, NodeType Type) : Node(Alloc, Type) {}
};

template<NodeType Kind>
class Nary final : public NaryNode {
  Nary(const Nary<Kind>&) = delete;
  Nary<Kind> &operator=(const Nary<Kind>&) = delete;
  virtual void forceCompilation() final;
public:
  Nary() : NaryNode(Kind) {}
  Nary(alloc::Allocator *Alloc) : NaryNode(Alloc, Kind) {}
  ~Nary() {}
};

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_SEXP_AST_H
