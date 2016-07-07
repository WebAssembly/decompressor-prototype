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
#include "sexp/Ast.def"
#include "stream/WriteUtils.h"
#include "Casting.h"

#include <array>
#include <limits>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace wasm {

using namespace decode;

namespace filt {

using ExternalName = std::string;
using InternalName = arena_vector<uint8_t>;

enum NodeType {
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
  Op##tag = opcode,
  AST_OPCODE_TABLE
#undef X
};

static constexpr size_t NumNodeTypes =
    0
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
    + 1
    AST_OPCODE_TABLE
#undef X
    ;

static constexpr size_t MaxNodeType =
    const_maximum(
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
        size_t(opcode),
  AST_OPCODE_TABLE
#undef X
  std::numeric_limits<size_t>::min());

struct AstTraitsType {
  const NodeType Type;
  const char *SexpName;
  const char *TypeName;
  const int NumTextArgs;
  const int AdditionalTextArgs;
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
  void forceCompilation();
public:
  using IndexType = size_t;
  class Iterator {
  public:
    explicit Iterator(const Node *Node, int Index)
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
    int Index;
  };

  virtual ~Node() {}

  NodeType getRtClassId() const {
    return Type;
  }

  NodeType getType() const {
    return Type;
  }

  // General API to children.
  virtual int getNumKids() const = 0;

  virtual Node *getKid(int Index) const = 0;

  virtual void setKid(int Index, Node *N) = 0;

  Node *getLastKid() const {
    if (int Size = getNumKids())
      return getKid(Size-1);
    return nullptr;
  }

  // WARNING: Only supported if underlying type allows.
  virtual void append(Node *Kid);

  // General iterators for walking kids.
  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, getNumKids()); }
  Iterator rbegin() const { return Iterator(this, getNumKids() - 1); }
  Iterator rend() const { return Iterator(this, -1); }

  static bool implementsClass(NodeType /*Type*/) { return true; }

protected:
  NodeType Type;
  Node(alloc::Allocator *, NodeType Type) : Type(Type) {}
};

class NullaryNode : public Node {
  NullaryNode(const NullaryNode&) = delete;
  NullaryNode &operator=(const NullaryNode&) = delete;
public:
  ~NullaryNode() override {}

  static bool implementsClass(NodeType Type);

  int getNumKids() const final {
    return 0;
  }

  Node *getKid(int Index) const final;

  void setKid(int Index, Node *N) final;

protected:
  NullaryNode(alloc::Allocator *Alloc, NodeType Type)
      : Node(Alloc, Type) {}
};

#define X(tag)                                                                 \
  class tag##Node final : public NullaryNode {                                 \
    tag##Node(const tag##Node&) = delete;                                      \
    tag##Node &operator=(const tag##Node&) = delete;                           \
    virtual void forceCompilation() final;                                     \
  public:                                                                      \
    tag##Node() : NullaryNode(alloc::Allocator::Default, Op##tag) {}           \
    explicit tag##Node(alloc::Allocator *Alloc)                                \
         : NullaryNode(Alloc, Op##tag) {}                                      \
    ~tag##Node() override {}                                                   \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; }     \
  };
  AST_NULLARYNODE_TABLE
#undef X

class IntegerNode final : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode &operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;
  virtual void forceCompilation() final;
public:
  // Note: ValueFormat provided so that we can echo back out same
  // representation as when lexing s-expressions.
#if 0
  enum ValueFormat { Decimal, SignedDecimal, Hexidecimal };
#endif
  IntegerNode(decode::IntType Value, ValueFormat Format = ValueFormat::Decimal) :
      NullaryNode(alloc::Allocator::Default, OpInteger),
      Value(Value),
      Format(Format) {}
  IntegerNode(alloc::Allocator *Alloc, decode::IntType Value,
              ValueFormat Format = ValueFormat::Decimal) :
      NullaryNode(Alloc, OpInteger),
      Value(Value),
      Format(Format) {}
  ~IntegerNode() override {}
  ValueFormat getFormat() const {
    return Format;
  }
  decode::IntType getValue() const {
    return Value;
  }

  static bool implementsClass(NodeType Type) { return Type == OpInteger; }

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
  explicit SymbolNode(ExternalName &_Name) :
      NullaryNode(alloc::Allocator::Default, OpSymbol),
      Name(alloc::Allocator::Default) {
    init(_Name);
  }
  SymbolNode(alloc::Allocator *Alloc, ExternalName &_Name)
      : NullaryNode(Alloc, OpSymbol), Name(Alloc) {
    init(_Name);
  }
  ~SymbolNode() override {}
  const InternalName &getName() const {
    return Name;
  }
  const std::string getStringName() const;
  const Node *getDefineDefinition() const {
    return DefineDefinition;
  }
  void setDefineDefinition(Node *Defn);
  const Node *getDefaultDefinition() const {
    return DefaultDefinition;
  }
  void setDefaultDefinition(Node *Defn);

  static bool implementsClass(NodeType Type) { return Type == OpSymbol; }

private:
  InternalName Name;
  Node *DefineDefinition = nullptr;
  Node *DefaultDefinition = nullptr;
  bool IsDefineUsingDefault = true;
  void init(ExternalName &_Name) {
    Name.reserve(Name.size());
    for (const auto &V : _Name)
      Name.emplace_back(V);
  }
};

class SymbolTable {
  SymbolTable(const SymbolTable &) = delete;
  SymbolTable &operator=(const SymbolTable &) = delete;
public:
  explicit SymbolTable(alloc::Allocator *Alloc);
  // Gets existing symbol if known. Otherwise returns nullptr.
  SymbolNode *getSymbol(ExternalName &Name) {
    return SymbolMap[Name];
  }
  // Gets existing symbol if known. Otherwise returns newly created symbol.
  // Used to keep symbols unique within filter s-expressions.
  SymbolNode *getSymbolDefinition(ExternalName &Name);
  // Install definitions in tree defined by root.
  void install(Node* Root);
  alloc::Allocator *getAllocator() const {
    return Alloc;
  }
private:
  alloc::Allocator *Alloc;
  Node *Error;
  // TODO(KarlSchimpf): Use arena allocator on map.
  std::map<ExternalName, SymbolNode*> SymbolMap;
};

class UnaryNode : public Node {
  UnaryNode(const UnaryNode&) = delete;
  UnaryNode &operator=(const UnaryNode&) = delete;
public:
  ~UnaryNode() override {}

  int getNumKids() const final {
    return 1;
  }

  Node *getKid(int Index) const final;

  void setKid(int Index, Node *N) final;

  static bool implementsClass(NodeType Type);

protected:
  Node *Kids[1];
  UnaryNode(alloc::Allocator *Alloc, NodeType Type, Node *Kid)
      : Node(Alloc, Type) {
    Kids[0] = Kid;
  }
};

#define X(tag)                                                                 \
  class tag##Node final : public UnaryNode {                                   \
    tag##Node(const tag##Node&) = delete;                                      \
    tag##Node &operator=(const tag##Node&) = delete;                           \
    virtual void forceCompilation() final;                                     \
  public:                                                                      \
    explicit tag##Node(Node *Kid)                                              \
      : UnaryNode(alloc::Allocator::Default, Op##tag, Kid) {}                  \
    tag##Node(alloc::Allocator* Alloc, Node *Kid)                              \
      : UnaryNode(Alloc, Op##tag, Kid) {}                                      \
    ~tag##Node() override {}                                                   \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; }     \
  };
  AST_UNARYNODE_TABLE
#undef X

class BinaryNode : public Node {
  BinaryNode(const BinaryNode&) = delete;
  BinaryNode &operator=(const BinaryNode&) = delete;
public:
  ~BinaryNode() override {}

  int getNumKids() const final {
    return 2;
  }

  Node *getKid(int Index) const final;

  void setKid(int Index, Node *N) final;

  static bool implementsClass(NodeType Type);

protected:
  Node *Kids[2];
  BinaryNode(NodeType Type, Node *Kid1, Node *Kid2)
      : Node(alloc::Allocator::Default, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
  BinaryNode(alloc::Allocator *Alloc, NodeType Type, Node *Kid1, Node *Kid2)
      : Node(Alloc, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
};

#define X(tag)                                                                 \
  class tag##Node final : public BinaryNode {                                  \
    tag##Node(const tag##Node&) = delete;                                      \
    tag##Node &operator=(const tag##Node&) = delete;                           \
    virtual void forceCompilation() final;                                     \
  public:                                                                      \
    tag##Node(Node *Kid1, Node *Kid2) : BinaryNode(Op##tag, Kid1, Kid2) {}     \
    tag##Node(alloc::Allocator *Alloc, Node *Kid1, Node *Kid2)                 \
        : BinaryNode(Alloc, Op##tag, Kid1, Kid2) {}                            \
    ~tag##Node() override {}                                                   \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; }     \
  };
  AST_BINARYNODE_TABLE
#undef X

class NaryNode : public Node {
  NaryNode(const NaryNode&) = delete;
  NaryNode &operator=(const NaryNode&) = delete;
  virtual void forceCompilation();
public:
  int getNumKids() const override final {
    return Kids.size();
  }

  Node *getKid(int Index) const override final {
    return Kids[Index];
  }

  void setKid(int Index, Node *N) override final {
    Kids[Index] = N;
  }

  void append(Node *Kid) final {
    Kids.emplace_back(Kid);
  }
  ~NaryNode() override {}

  static bool implementsClass(NodeType Type);

protected:
  arena_vector<Node*> Kids;
  explicit NaryNode(NodeType Type) : Node(alloc::Allocator::Default, Type) {}
  NaryNode(alloc::Allocator *Alloc, NodeType Type)
      : Node(Alloc, Type), Kids(alloc::TemplateAllocator<Node*>(Alloc)) {}
};

class SelectNode final : public NaryNode {
  SelectNode(const SelectNode &) = delete;
  SelectNode &operator=(const SelectNode &) = delete;
  virtual void forceCompilation() final;
public:
  SelectNode() : NaryNode(OpSelect) {}
  explicit SelectNode(alloc::Allocator *Alloc)
      : NaryNode(Alloc, OpSelect) {}
  static bool implementsClass(NodeType Type) { return OpSelect == Type; }
  void installFastLookup();
  const Node *getCase(IntType Key) const;
private:
  // TODO(kschimpf) Hook this up to allocator.
  std::unordered_map<IntType, Node *> LookupMap;
};

#define X(tag)                                                                 \
  class tag##Node final : public NaryNode {                                    \
    tag##Node(const tag##Node&) = delete;                                      \
    tag##Node &operator=(const tag##Node&) = delete;                           \
    virtual void forceCompilation() final;                                     \
  public:                                                                      \
    tag##Node() : NaryNode(Op##tag) {}                                         \
    explicit tag##Node(alloc::Allocator *Alloc) : NaryNode(Alloc, Op##tag) {}  \
    ~tag##Node() override {}                                                   \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; }     \
  };
  AST_NARYNODE_TABLE
#undef X

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_SEXP_AST_H
