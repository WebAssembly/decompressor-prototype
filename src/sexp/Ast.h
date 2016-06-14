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

/* Defines an internal model of filter AST's. */

#ifndef DECOMPRESSOR_SRC_SEXP_AST_H
#define DECOMPRESSOR_SRC_SEXP_AST_H

#include "Defs.h"

// #include <iterator>
#include <memory>
#include <vector>

namespace wasm {

namespace filt {

enum class NodeType {
  Append,
    AppendValue,
    AstToBit,
    AstToByte,
    AstToInt,
    BitToAst,
    BitToBit,
    BitToByte,
    BitToInt,
    ByteToAst,
    ByteToBit,
    ByteToByte,
    ByteToInt,
    Call,
    Case,
    Copy,
    Define,
    Eval,
    Extract,
    ExtractBegin,
    ExtractEnd,
    ExtractEof,
    File,
    Filter,
    Fixed32,
    Fixed64,
    IfThenElse,
    Integer,
    IntToAst,
    IntToBit,
    IntToByte,
    IntToInt,
    I32Const,
    I64Const,
    Lit,
    Loop,
    LoopUnbounded,
    Map,
    Method,
    Peek,
    Postorder,
    Preorder,
    Read,
    Section,
    Select,
    Sequence,
    Symbol,
    SymConst,
    Uint32,
    Uint8,
    Uint64,
    U32Const,
    U64Const,
    Value,
    Varint32,
    Varint64,
    Varuint1,
    Varuint7,
    Varuint32,
    Varuint64,
    Vbrint32,
    Vbrint64,
    Vbruint32,
    Vbruint64,
    Version,
    Void,
    Write // Assumed to be last in list (see NumNodeTypes).
};

static constexpr size_t NumNodeTypes = static_cast<int>(NodeType::Write) + 1;

const char *getNodeTypeName(NodeType Type);

class Node;

// Holds memory associated with nodes.
// TODO: Make this an arena allocator.
class NodeMemory {
  NodeMemory(const NodeMemory&) = delete;
  NodeMemory &operator=(const NodeMemory&) = delete;
public:
  NodeMemory() {}
  ~NodeMemory() {}
  static NodeMemory Default;
private:
  friend class Node;
  void add(Node *N) {
    std::unique_ptr<Node> Nptr(N);
    Nodes.push_back(std::move(Nptr));
  }
  std::vector<std::unique_ptr<Node>> Nodes;
};

class Node {
  Node(const Node&) = delete;
  Node &operator=(const Node&) = delete;
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
  virtual IndexType getNumKids() const = 0;
  virtual Node *getKid(IndexType Index) const = 0;
  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, getNumKids()); }
  Iterator rbegin() { return Iterator(this, getNumKids() - 1); }
  Iterator rend() const { return Iterator(this, -1); }
protected:
  const NodeType Type;
  Node(NodeType Type) : Type(Type) {}
  Node *add(NodeMemory &Memory) {
    Memory.add(this);
    return this;
  }
};

class NullaryNode : public Node {
  NullaryNode(const NullaryNode&) = delete;
  NullaryNode &operator=(const NullaryNode&) = delete;
public:
  IndexType getNumKids() const final { return 0; }
  Node *getKid(IndexType /*Index*/) const final {
    assert(false);
    return nullptr;
  }
  ~NullaryNode() override {}
protected:
  NullaryNode(NodeType Type) : Node(Type) {}
};

template<NodeType Kind>
class Nullary final : public NullaryNode {
  Nullary(const Nullary<Kind>&) = delete;
  Nullary<Kind> &operator=(const Nullary<Kind>&) = delete;
  virtual void forceCompilation();
public:
  static Nullary<Kind> *create(
      NodeMemory &Memory = NodeMemory::Default) {
    Nullary<Kind> *Node = new Nullary<Kind>();
    Node->add(Memory);
    return Node;
  }
  ~Nullary() {}
private:
  Nullary() : NullaryNode(Kind) {}
};

class IntegerNode final : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode &operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;
  virtual void forceCompilation();
public:
  ~IntegerNode() {}
  static IntegerNode *create(
      decode::IntType Value,
      NodeMemory &Memory = NodeMemory::Default) {
    IntegerNode *Node = new IntegerNode(Value);
    Node->add(Memory);
    return Node;
  }
  decode::IntType getValue() const {
    return Value;
  }
private:
  decode::IntType Value;
  IntegerNode(decode::IntType Value)
      : NullaryNode(NodeType::Integer), Value(Value) {}
};

class SymbolNode final : public NullaryNode {
  SymbolNode(const SymbolNode&) = delete;
  SymbolNode &operator=(const SymbolNode&) = delete;
  SymbolNode() = delete;
  virtual void forceCompilation();
public:
  ~SymbolNode() {}
  static SymbolNode *create(
      std::string Name,
      NodeMemory &Memory = NodeMemory::Default) {
    SymbolNode *Node = new SymbolNode(Name);
    Node->add(Memory);
    return Node;
  }
  std::string getName() const {
    return Name;
  }
private:
  std::string Name;
  SymbolNode(std::string Name)
      : NullaryNode(NodeType::Symbol), Name(Name) {}
};

class UnaryNode : public Node {
  UnaryNode(const UnaryNode&) = delete;
  UnaryNode &operator=(const UnaryNode&) = delete;
public:
  IndexType getNumKids() const final { return 1; }
  Node *getKid(IndexType Index) const final {
    assert(Index == 0);
    return Kid;
  }
  ~UnaryNode() override {}
protected:
  Node *Kid;
  UnaryNode(NodeType Type, Node *Kid)
      : Node(Type), Kid(Kid) {}
};


template<NodeType Kind>
class Unary final : public UnaryNode {
  Unary(const Unary<Kind>&) = delete;
  Unary<Kind> &operator=(const Unary<Kind>&) = delete;
  virtual void forceCompilation();
public:
  ~Unary() {}
  static Unary<Kind> *create(
      Node *Kid, NodeMemory &Memory = NodeMemory::Default) {
    Unary<Kind> *Node = new Unary<Kind>(Kid);
    Node->add(Memory);
    return Node;
  }
private:
  Unary(Node *Kid) : UnaryNode(Kind, Kid) {}
};


class BinaryNode : public Node {
  BinaryNode(const BinaryNode&) = delete;
  BinaryNode &operator=(const BinaryNode&) = delete;
public:
  IndexType getNumKids() const final { return 2; }
  Node *getKid(IndexType Index) const final {
    assert(Index < 2);
    return Kids[Index];
  }
  ~BinaryNode() override {}
protected:
  Node *Kids[2];
  BinaryNode(NodeType Type, Node *Kid1, Node *Kid2)
      : Node(Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
};

template<NodeType Kind>
class Binary final : public BinaryNode {
  Binary(const Binary<Kind>&) = delete;
  Binary<Kind> &operator=(const Binary<Kind>&) = delete;
  virtual void forceCompilation();
public:
  ~Binary() {}
  static Binary<Kind> *create(
      Node *Kid1, Node *Kid2, NodeMemory &Memory = NodeMemory::Default) {
    Binary<Kind> *Node = new Binary<Kind>(Kid1, Kid2);
    Node->add(Memory);
    return Node;
  }
private:
  Binary(Node *Kid1, Node *Kid2) : BinaryNode(Kind, Kid1, Kid2) {}
};

class IfThenElse final : public Node {
  IfThenElse(const IfThenElse&) = delete;
  IfThenElse &operator=(const IfThenElse&) = delete;
  IfThenElse() = delete;
  virtual void forceCompilation();
public:
  ~IfThenElse() {}
  IndexType getNumKids() const final { return 3; }
  Node *getKid(IndexType Index) const final {
    assert(Index < 3);
    return Kids[Index];
  }
  static IfThenElse *create(
      Node *Exp, Node *Then, Node *Else,
      NodeMemory &Memory = NodeMemory::Default) {
    IfThenElse *Node = new IfThenElse(Exp, Then, Else);
    Node->add(Memory);
    return Node;
  }
  Node *getTest() const { return Kids[0]; }
  Node *getThen() const { return Kids[1]; }
  Node *getElse() const { return Kids[2]; }
private:
  Node *Kids[3];
  IfThenElse(Node *Exp, Node* Then, Node* Else)
      : Node(NodeType::IfThenElse) {
    Kids[0] = Exp;
    Kids[1] = Then;
    Kids[2] = Else;
  }
};

class NaryNode : public Node {
  NaryNode(const NaryNode&) = delete;
  NaryNode &operator=(const NaryNode&) = delete;
public:
  IndexType getNumKids() const final { return Kids.size(); }
  Node *getKid(IndexType Index) const final {
    return Kids.at(Index);
  }
  void append(Node *Kid) { Kids.push_back(Kid); }
  ~NaryNode() override {}
protected:
  std::vector<Node*> Kids;
  NaryNode(NodeType Type) : Node(Type) {}
};


template<NodeType Kind>
class Nary final : public NaryNode {
  Nary(const Nary<Kind>&) = delete;
  Nary<Kind> &operator=(const Nary<Kind>&) = delete;
  virtual void forceCompilation();
public:
  ~Nary() {}
  static Nary<Kind> *create(
      NodeMemory &Memory = NodeMemory::Default) {
    Nary<Kind> *Node = new Nary<Kind>();
    Node->add(Memory);
    return Node;
  }
private:
  Nary() : NaryNode(Kind) {}
};

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_SEXP_AST_H
