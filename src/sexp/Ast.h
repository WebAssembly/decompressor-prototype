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
//    virtual void forceCompilation() FINAL;
//
// This is done to force the compilation of virtuals associated with the
// class in file Ast.cpp.
//
// Note: Classes allow an optional allocator as the first argument. This
// allows the creator to decide what allocator will be used internally.

#ifndef DECOMPRESSOR_SRC_SEXP_AST_H
#define DECOMPRESSOR_SRC_SEXP_AST_H

#include "ADT/arena_vector.h"
#include "sexp/Ast.def"
#include "sexp/TraceSexp.h"
#include "stream/WriteUtils.h"
#include "utils/Allocator.h"
#include "utils/Casting.h"
#include "utils/Defs.h"

#include <array>
#include <limits>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wasm {

namespace filt {

class Node;
class SymbolTable;

typedef std::string ExternalName;
typedef ARENA_VECTOR(uint8_t) InternalName;
typedef std::unordered_set<Node*> VisitedNodesType;
typedef std::vector<Node*> NodeVectorType;

enum NodeType {
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
  Op##tag = opcode,
  AST_OPCODE_TABLE
#undef X
      NO_SUCH_NODETYPE
};

static constexpr size_t NumNodeTypes = 0
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) +1
    AST_OPCODE_TABLE
#undef X
    ;

static constexpr size_t MaxNodeType = const_maximum(
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
  size_t(opcode),
    AST_OPCODE_TABLE
#undef X
        std::numeric_limits<size_t>::min());

struct AstTraitsType {
  const NodeType Type;
  const char* SexpName;
  const char* TypeName;
  const int NumTextArgs;
  const int AdditionalTextArgs;
};

extern AstTraitsType AstTraits[NumNodeTypes];

// Returns the s-expression name
const char* getNodeSexpName(NodeType Type);

// Returns a unique (printable) type name
const char* getNodeTypeName(NodeType Type);

class Node {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node() = delete;
  void forceCompilation();
  friend class SymbolTable;
 public:
  typedef size_t IndexType;
  class Iterator {
   public:
    explicit Iterator(const Node* Nd, int Index) : Nd(Nd), Index(Index) {}
    Iterator(const Iterator& Iter) : Nd(Iter.Nd), Index(Iter.Index) {}
    Iterator& operator=(const Iterator& Iter) {
      Nd = Iter.Nd;
      Index = Iter.Index;
      return *this;
    }
    void operator++() { ++Index; }
    void operator--() { --Index; }
    bool operator==(const Iterator& Iter) {
      return Nd == Iter.Nd && Index == Iter.Index;
    }
    bool operator!=(const Iterator& Iter) {
      return Nd != Iter.Nd || Index != Iter.Index;
    }
    Node* operator*() const { return Nd->getKid(Index); }

   private:
    const Node* Nd;
    int Index;
  };

  // Turns on ability to get warning/error messages.
  static TraceClassSexp Trace;

  virtual ~Node() {}

  NodeType getRtClassId() const { return Type; }

  NodeType getType() const { return Type; }

  // General API to children.
  virtual int getNumKids() const = 0;

  virtual Node* getKid(int Index) const = 0;

  virtual void setKid(int Index, Node* N) = 0;

  void setLastKid(Node* N) { setKid(getNumKids() - 1, N); }

  Node* getLastKid() const {
    if (int Size = getNumKids())
      return getKid(Size - 1);
    return nullptr;
  }

  // WARNING: Only supported if underlying type allows.
  virtual void append(Node* Kid);

  // General iterators for walking kids.
  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, getNumKids()); }
  Iterator rbegin() const { return Iterator(this, getNumKids() - 1); }
  Iterator rend() const { return Iterator(this, -1); }

  static bool implementsClass(NodeType /*Type*/) { return true; }

 protected:
  NodeType Type;
  Node(alloc::Allocator*, NodeType Type) : Type(Type) {}
  virtual void clearCaches(NodeVectorType &AdditionalNodes);
  virtual void installCaches(NodeVectorType &AdditionalNodes);
};

class NullaryNode : public Node {
  NullaryNode(const NullaryNode&) = delete;
  NullaryNode& operator=(const NullaryNode&) = delete;

 public:
  ~NullaryNode() OVERRIDE {}

  static bool implementsClass(NodeType Type);

  int getNumKids() const FINAL { return 0; }

  Node* getKid(int Index) const FINAL;

  void setKid(int Index, Node* N) FINAL;

 protected:
  NullaryNode(alloc::Allocator* Alloc, NodeType Type) : Node(Alloc, Type) {}
};

#define X(tag)                                                             \
  class tag##Node FINAL : public NullaryNode {                             \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation() FINAL;                                 \
                                                                           \
   public:                                                                 \
    tag##Node() : NullaryNode(alloc::Allocator::Default, Op##tag) {}       \
    explicit tag##Node(alloc::Allocator* Alloc)                            \
        : NullaryNode(Alloc, Op##tag) {}                                   \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; } \
  };
AST_NULLARYNODE_TABLE
#undef X

class IntegerNode FINAL : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode& operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;
  virtual void forceCompilation() FINAL;

 public:
  // Note: ValueFormat provided so that we can echo back out same
  // representation as when lexing s-expressions.
  IntegerNode(decode::IntType Value,
              decode::ValueFormat Format = decode::ValueFormat::Decimal)
      : NullaryNode(alloc::Allocator::Default, OpInteger),
        Value(Value),
        Format(Format) {}
  IntegerNode(alloc::Allocator* Alloc,
              decode::IntType Value,
              decode::ValueFormat Format = decode::ValueFormat::Decimal)
      : NullaryNode(Alloc, OpInteger), Value(Value), Format(Format) {}
  ~IntegerNode() OVERRIDE {}
  decode::ValueFormat getFormat() const { return Format; }
  decode::IntType getValue() const { return Value; }

  static bool implementsClass(NodeType Type) { return Type == OpInteger; }

 private:
  decode::IntType Value;
  decode::ValueFormat Format;
};

class SymbolNode FINAL : public NullaryNode {
  SymbolNode(const SymbolNode&) = delete;
  SymbolNode& operator=(const SymbolNode&) = delete;
  SymbolNode() = delete;
  virtual void forceCompilation() FINAL;

 public:
  explicit SymbolNode(ExternalName& _Name)
      : NullaryNode(alloc::Allocator::Default, OpSymbol),
        Name(alloc::Allocator::Default) {
    init(_Name);
  }
  SymbolNode(alloc::Allocator* Alloc, ExternalName& _Name)
      : NullaryNode(Alloc, OpSymbol), Name(Alloc) {
    init(_Name);
  }
  ~SymbolNode() OVERRIDE {}
  const InternalName& getName() const { return Name; }
  std::string getStringName() const;
  const Node* getDefineDefinition() const { return DefineDefinition; }
  void setDefineDefinition(Node* Defn);
  const Node* getDefaultDefinition() const { return DefaultDefinition; }
  void setDefaultDefinition(Node* Defn);

  static bool implementsClass(NodeType Type) { return Type == OpSymbol; }

 private:
  InternalName Name;
  Node* DefineDefinition;
  Node* DefaultDefinition;
  bool IsDefineUsingDefault;
  void init(ExternalName& _Name) {
    Name.reserve(Name.size());
    for (const auto& V : _Name)
      Name.emplace_back(V);
    DefineDefinition = nullptr;
    DefaultDefinition = nullptr;
    IsDefineUsingDefault = true;
  }
  void clearCaches(NodeVectorType &AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType &AdditionalNodes) OVERRIDE;
};

class SymbolTable {
  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;

 public:
  explicit SymbolTable(alloc::Allocator* Alloc);
  ~SymbolTable() { clear(); }
  // Gets existing symbol if known. Otherwise returns nullptr.
  SymbolNode* getSymbol(ExternalName& Name) { return SymbolMap[Name]; }
  // Gets existing symbol if known. Otherwise returns newly created symbol.
  // Used to keep symbols unique within filter s-expressions.
  SymbolNode* getSymbolDefinition(ExternalName& Name);
  // Install definitions in tree defined by root.
  void install(Node* Root);
  alloc::Allocator* getAllocator() const { return Alloc; }
  void clear() { SymbolMap.clear(); }

 private:
  alloc::Allocator* Alloc;
  Node* Error;
  // TODO(KarlSchimpf): Use arena allocator on map.
  std::map<ExternalName, SymbolNode*> SymbolMap;
  void installDefinitions(Node* Root);
  void clearSubtreeCaches(Node *Nd, VisitedNodesType &VisitedNodes,
                          NodeVectorType &AdditionalNodes);
  void installSubtreeCaches(Node *Nd, VisitedNodesType &VisitedNoes,
                            NodeVectorType &AdditionalNodes);
};

class UnaryNode : public Node {
  UnaryNode(const UnaryNode&) = delete;
  UnaryNode& operator=(const UnaryNode&) = delete;

 public:
  ~UnaryNode() OVERRIDE {}

  int getNumKids() const FINAL { return 1; }

  Node* getKid(int Index) const FINAL;

  void setKid(int Index, Node* N) FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[1];
  UnaryNode(alloc::Allocator* Alloc, NodeType Type, Node* Kid)
      : Node(Alloc, Type) {
    Kids[0] = Kid;
  }
};

#define X(tag)                                                             \
  class tag##Node FINAL : public UnaryNode {                               \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation() FINAL;                                 \
                                                                           \
   public:                                                                 \
    explicit tag##Node(Node* Kid)                                          \
        : UnaryNode(alloc::Allocator::Default, Op##tag, Kid) {}            \
    tag##Node(alloc::Allocator* Alloc, Node* Kid)                          \
        : UnaryNode(Alloc, Op##tag, Kid) {}                                \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
  };
AST_UNARYNODE_TABLE
#undef X

class BinaryNode : public Node {
  BinaryNode(const BinaryNode&) = delete;
  BinaryNode& operator=(const BinaryNode&) = delete;

 public:
  ~BinaryNode() OVERRIDE {}

  int getNumKids() const FINAL { return 2; }

  Node* getKid(int Index) const FINAL;

  void setKid(int Index, Node* N) FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[2];
  BinaryNode(NodeType Type, Node* Kid1, Node* Kid2)
      : Node(alloc::Allocator::Default, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
  BinaryNode(alloc::Allocator* Alloc, NodeType Type, Node* Kid1, Node* Kid2)
      : Node(Alloc, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
};

#define X(tag)                                                             \
  class tag##Node FINAL : public BinaryNode {                              \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation() FINAL;                                 \
                                                                           \
   public:                                                                 \
    tag##Node(Node* Kid1, Node* Kid2) : BinaryNode(Op##tag, Kid1, Kid2) {} \
    tag##Node(alloc::Allocator* Alloc, Node* Kid1, Node* Kid2)             \
        : BinaryNode(Alloc, Op##tag, Kid1, Kid2) {}                        \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
  };
AST_BINARYNODE_TABLE
#undef X

class TernaryNode : public Node {
  TernaryNode(const TernaryNode&) = delete;
  TernaryNode& operator=(const TernaryNode&) = delete;

 public:
  ~TernaryNode() OVERRIDE {}

  int getNumKids() const FINAL { return 3; }

  Node* getKid(int Index) const FINAL;

  void setKid(int Index, Node* N) FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[3];
  TernaryNode(NodeType Type, Node* Kid1, Node* Kid2, Node* Kid3)
      : Node(alloc::Allocator::Default, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
    Kids[2] = Kid3;
  }
  TernaryNode(alloc::Allocator* Alloc,
              NodeType Type,
              Node* Kid1,
              Node* Kid2,
              Node* Kid3)
      : Node(Alloc, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
    Kids[1] = Kid3;
  }
};

#define X(tag)                                                             \
  class tag##Node FINAL : public TernaryNode {                             \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation() FINAL;                                 \
                                                                           \
   public:                                                                 \
    tag##Node(Node* Kid1, Node* Kid2, Node* Kid3)                          \
        : TernaryNode(Op##tag, Kid1, Kid2, Kid3) {}                        \
    tag##Node(alloc::Allocator* Alloc, Node* Kid1, Node* Kid2, Node* Kid3) \
        : TernaryNode(Alloc, Op##tag, Kid1, Kid2, Kid3) {}                 \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
  };
AST_TERNARYNODE_TABLE
#undef X

class NaryNode : public Node {
  NaryNode(const NaryNode&) = delete;
  NaryNode& operator=(const NaryNode&) = delete;
  virtual void forceCompilation();

 public:
  int getNumKids() const OVERRIDE FINAL { return Kids.size(); }

  Node* getKid(int Index) const OVERRIDE FINAL { return Kids[Index]; }

  void setKid(int Index, Node* N) OVERRIDE FINAL { Kids[Index] = N; }

  void append(Node* Kid) FINAL { Kids.emplace_back(Kid); }
  ~NaryNode() OVERRIDE {}

  static bool implementsClass(NodeType Type);

 protected:
  ARENA_VECTOR(Node*) Kids;
  explicit NaryNode(NodeType Type) : Node(alloc::Allocator::Default, Type) {}
  NaryNode(alloc::Allocator* Alloc, NodeType Type)
      : Node(Alloc, Type), Kids(alloc::TemplateAllocator<Node*>(Alloc)) {}
};

#define X(tag)                                                                \
  class tag##Node FINAL : public NaryNode {                                   \
    tag##Node(const tag##Node&) = delete;                                     \
    tag##Node& operator=(const tag##Node&) = delete;                          \
    virtual void forceCompilation() FINAL;                                    \
                                                                              \
   public:                                                                    \
    tag##Node() : NaryNode(Op##tag) {}                                        \
    explicit tag##Node(alloc::Allocator* Alloc) : NaryNode(Alloc, Op##tag) {} \
    ~tag##Node() OVERRIDE {}                                                  \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; }    \
  };
AST_NARYNODE_TABLE
#undef X

class SelectBaseNode : public NaryNode {
  SelectBaseNode(const SelectBaseNode&) = delete;
  SelectBaseNode& operator=(const SelectBaseNode&) = delete;
  virtual void forceCompilation();

 public:
  void installFastLookup();
  const CaseNode* getCase(decode::IntType Key) const;

 protected:
  // TODO(karlschimpf) Hook this up to allocator.
  std::unordered_map<decode::IntType, const CaseNode*> LookupMap;

  SelectBaseNode(NodeType Type) : NaryNode(Type) {}
  SelectBaseNode(alloc::Allocator* Alloc, NodeType Type)
      : NaryNode(Alloc, Type) {}
};

#define X(tag)                                                             \
  class tag##Node FINAL : public SelectBaseNode {                          \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation() FINAL;                                 \
                                                                           \
   public:                                                                 \
    tag##Node() : SelectBaseNode(Op##tag) {}                               \
    explicit tag##Node(alloc::Allocator* Alloc)                            \
        : SelectBaseNode(Alloc, Op##tag) {}                                \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
  };
AST_SELECTNODE_TABLE
#undef X

class OpcodeNode FINAL : public SelectBaseNode {
  OpcodeNode(const OpcodeNode&) = delete;
  OpcodeNode& operator=(const OpcodeNode&) = delete;
  virtual void forceCompilation() FINAL;

 public:
  OpcodeNode() : SelectBaseNode(OpOpcode) {}
  explicit OpcodeNode(alloc::Allocator* Alloc)
      : SelectBaseNode(Alloc, OpOpcode) {}
  static bool implementsClass(NodeType Type) { return OpOpcode == Type; }
  const CaseNode *getWriteCase(decode::IntType Value,
                               uint32_t &SelShift,
                               decode::IntType &CaseMask) const;
private:
  // Associates Opcode Case's with range [Min, Max].
  //
  // Note: Code assumes that shift value and case key is implicitly
  // encoded into range [Min, Max]. Therefore, they are not used in
  // comparison.
  class WriteRange {
    WriteRange() = delete;
   public:
    WriteRange(const CaseNode *Case, decode::IntType Min,
               decode::IntType Max, uint32_t ShiftValue)
        : Case(Case), Min(Min), Max(Max), ShiftValue(ShiftValue) {}
    ~WriteRange() {}
    const CaseNode* getCase() const { return Case; }
    decode::IntType getMin() const { return Min; }
    decode::IntType getMax() const { return Max; }
    uint32_t getShiftValue() const { return ShiftValue; }
    int compare(const WriteRange &R) const;
    bool operator<(const WriteRange &R) const {
      return compare(R) < 0;
    }
    void trace() const {
      if (Trace.getTraceProgress())
        traceInternal("");
    }
    void trace(const char* Prefix) const {
      if (Trace.getTraceProgress())
        traceInternal(Prefix);
    }
   private:
    const CaseNode *Case;
    decode::IntType Min;
    decode::IntType Max;
    uint32_t ShiftValue;
    void traceInternal(const char* Prefix) const;
  };
  void clearCaches(NodeVectorType &AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType &AdditionalNodes) OVERRIDE;
  typedef std::vector<WriteRange> CaseRangeVectorType;
  CaseRangeVectorType CaseRangeVector;
  void installCaseRanges();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_AST_H
