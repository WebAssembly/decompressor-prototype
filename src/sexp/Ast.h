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

#include <iterator>
#include <memory>
#include <vector>

namespace wasm {

namespace filt {

enum class NodeType {
  Integer,
  Symbol
};

class Node;

// Holds memory associated with nodes.
class NodeMemory {
  NodeMemory(const NodeMemory&) = delete;
  NodeMemory &operator=(const NodeMemory&) = delete;
public:
  NodeMemory() {}
  ~NodeMemory() {}
  static NodeMemory Default;
private:
  friend class Node;
  Node *add(Node *N) {
    std::unique_ptr<Node> Nptr(N);
    Nodes.push_back(std::move(Nptr));
    return N;
  }
  std::vector<std::unique_ptr<Node>> Nodes;
};

class Node {
  Node(const Node&) = delete;
  Node &operator=(const Node&) = delete;
public:
  class Iterator {
  public:
    explicit Iterator(const Node *Node, size_t Index): Node(Node), Index(Index) {}
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
    size_t Index;
  };

  NodeType getType() const {
    return Type;
  }
  virtual size_t getNumKids() const = 0;
  virtual Node *getKid(size_t Index) const = 0;
  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, getNumKids()); }
  void huh();
protected:
  NodeType Type;

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
  size_t getNumKids() const override { return 0; }
  Node *getKid(size_t Index) const override;
protected:
  NullaryNode(NodeType Type) : Node(Type) {}
};

class IntegerNode : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode &operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;
public:
  static Node *create(decode::IntType Value,
                      NodeMemory &Memory = NodeMemory::Default) {
    return (new IntegerNode(Value))->add(Memory);
  }
  decode::IntType getValue() const {
    return Value;
  }
private:
  decode::IntType Value;
  IntegerNode(decode::IntType Value)
      : NullaryNode(NodeType::Integer), Value(Value) {}
};

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_SEXP_AST_H
