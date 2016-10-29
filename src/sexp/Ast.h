// -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Defines an internal model of filter AST's.
//
// NOTE: template classes define  virtual:
//
//    virtual void forceCompilation();
//
// This is done to force the compilation of virtuals associated with the
// class to be put in file Ast.cpp.
//
// Note: Classes allow an optional allocator as the first argument. This
// allows the creator to decide what allocator will be used internally.

#ifndef DECOMPRESSOR_SRC_SEXP_AST_H
#define DECOMPRESSOR_SRC_SEXP_AST_H

#include "sexp/Ast.def"
#include "sexp/NodeType.h"
#include "sexp/Strings.def"
#include "sexp/TraceSexp.h"
#include "stream/WriteUtils.h"
#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/initialized_ptr.h"

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

class IntegerNode;
class Node;
class SymbolNode;
class SymbolTable;
class CallbackNode;

#define X(tag, format, defval, mergable, NODE_DECLS) class tag##Node;
AST_INTEGERNODE_TABLE
#undef X

typedef std::unordered_set<Node*> VisitedNodesType;
typedef std::vector<Node*> NodeVectorType;
typedef std::vector<const Node*> ConstNodeVectorType;

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

// Models integer values (as used in AST nodes).
class IntegerValue {
 public:
  IntegerValue()
      : Type(NO_SUCH_NODETYPE),
        Value(0),
        Format(decode::ValueFormat::Decimal),
        isDefault(false) {}
  explicit IntegerValue(decode::IntType Value, decode::ValueFormat Format)
      : Type(NO_SUCH_NODETYPE),
        Value(Value),
        Format(Format),
        isDefault(false) {}
  IntegerValue(NodeType Type,
               decode::IntType Value,
               decode::ValueFormat Format,
               bool isDefault = false)
      : Type(Type), Value(Value), Format(Format), isDefault(isDefault) {}
  explicit IntegerValue(const IntegerValue& V)
      : Type(V.Type),
        Value(V.Value),
        Format(V.Format),
        isDefault(V.isDefault) {}
  virtual int compare(const IntegerValue& V) const;
  bool operator<(const IntegerValue& V) const { return compare(V) < 0; }
  bool operator<=(const IntegerValue& V) const { return compare(V) <= 0; }
  bool operator==(const IntegerValue& V) const { return compare(V) == 0; }
  bool operator!=(const IntegerValue& V) const { return compare(V) != 0; }
  bool operator>=(const IntegerValue& V) const { return compare(V) >= 0; }
  bool operator>(const IntegerValue& V) const { return compare(V) > 0; }
  NodeType Type;
  decode::IntType Value;
  decode::ValueFormat Format;
  bool isDefault;
  // For debugging.
  virtual void describe(FILE* Out) const;
};

// Defines constants for predefined symbols (i.e. instances of SymbolNode
enum class PredefinedSymbol : uint32_t {
  Unknown
#define X(tag, name) , tag
      PREDEFINED_SYMBOLS_TABLE
#undef X
};

static constexpr uint32_t NumPredefinedSymbols = 1
#define X(tag, name) +1
    PREDEFINED_SYMBOLS_TABLE
#undef X
    ;

static constexpr PredefinedSymbol MaxPredefinedSymbol =
    PredefinedSymbol(NumPredefinedSymbols - 1);

extern PredefinedSymbol toPredefinedSymbol(uint32_t Value);
extern const char* getName(PredefinedSymbol);

// TODO(karlschimpf): Code no longer uses allocator. Remove from API.
class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;

 public:
  explicit SymbolTable();
  ~SymbolTable();
  // Gets existing symbol if known. Otherwise returns nullptr.
  SymbolNode* getSymbol(const std::string& Name) { return SymbolMap[Name]; }
  SymbolNode* getPredefined(PredefinedSymbol Sym) {
    return Predefined[uint32_t(Sym)];
  }
  // Gets existing symbol if known. Otherwise returns newly created symbol.
  // Used to keep symbols unique within filter s-expressions.
  SymbolNode* getSymbolDefinition(const std::string& Name);

  // Gets integer node (as defined by the arguments) if known. Otherwise
  // returns newly created integer.
#define X(tag, format, defval, mergable, NODE_DECLS)           \
  tag##Node* get##tag##Definition(decode::IntType Value,       \
                                  decode::ValueFormat Format); \
  tag##Node* get##tag##Definition();
  AST_INTEGERNODE_TABLE
#undef X
  // Gets actions corresponding to enter/exit block.
  const CallbackNode* getBlockEnterCallback() const {
    return BlockEnterCallback;
  }
  const CallbackNode* getBlockExitCallback() const { return BlockExitCallback; }
  // Install definitions in tree defined by root.
  void install(Node* Root);
  void clear() { SymbolMap.clear(); }
  int getNextCreationIndex() { return ++NextCreationIndex; }

  template <typename T, typename... Args>
  T* create(Args&&... args) {
    T* Nd = new T(*this, std::forward<Args>(args)...);
    Allocated.push_back(Nd);
    return Nd;
  }

  static bool installPredefinedDefaults(std::shared_ptr<SymbolTable> Symtab,
                                        bool Verbose = false);

  TraceClassSexp& getTrace() { return Trace; }

 private:
  std::vector<Node*> Allocated;
  TraceClassSexp Trace;
  Node* Error;
  int NextCreationIndex;
  std::map<std::string, SymbolNode*> SymbolMap;
  std::map<IntegerValue, IntegerNode*> IntMap;
  std::vector<SymbolNode*> Predefined;
  CallbackNode* BlockEnterCallback;
  CallbackNode* BlockExitCallback;

  void init();

  void installDefinitions(Node* Root);
  void clearSubtreeCaches(Node* Nd,
                          VisitedNodesType& VisitedNodes,
                          NodeVectorType& AdditionalNodes);
  void installSubtreeCaches(Node* Nd,
                            VisitedNodesType& VisitedNoes,
                            NodeVectorType& AdditionalNodes);
};

class Node {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node() = delete;
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

  virtual ~Node() {}

  NodeType getRtClassId() const { return Type; }

  NodeType getType() const { return Type; }

  const char* getName() const { return getNodeSexpName(getType()); }

  const char* getNodeName() const { return getNodeTypeName(getType()); }

  TraceClassSexp& getTrace() const { return Symtab.getTrace(); }

  bool hasKids() const { return getNumKids() > 0; }

  // General API to children.
  virtual int getNumKids() const = 0;

  virtual Node* getKid(int Index) const = 0;

  virtual void setKid(int Index, Node* N) = 0;

  // Validates the node, based on the parents.
  virtual bool validateNode(NodeVectorType& Parents);

  // Recursively walks tree and validates (Scope allows lexical validation).
  // Returns true if validation succeeds.
  bool validateSubtree(NodeVectorType& Parents);

  void setLastKid(Node* N) { setKid(getNumKids() - 1, N); }

  Node* getLastKid() const {
    if (int Size = getNumKids())
      return getKid(Size - 1);
    return nullptr;
  }

  // WARNING: Only supported if underlying type allows.
  virtual void append(Node* Kid);

  int getCreationIndex() const { return CreationIndex; }

  // General iterators for walking kids.
  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, getNumKids()); }
  Iterator rbegin() const { return Iterator(this, getNumKids() - 1); }
  Iterator rend() const { return Iterator(this, -1); }

  static bool implementsClass(NodeType /*Type*/) { return true; }

 protected:
  NodeType Type;
  SymbolTable& Symtab;
  int CreationIndex;
  Node(SymbolTable& Symtab, NodeType Type)
      : Type(Type),
        Symtab(Symtab),
        CreationIndex(Symtab.getNextCreationIndex()) {}
  virtual void clearCaches(NodeVectorType& AdditionalNodes);
  virtual void installCaches(NodeVectorType& AdditionalNodes);
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
  NullaryNode(SymbolTable& Symtab, NodeType Type) : Node(Symtab, Type) {}
};

#define X(tag, NODE_DECLS)                                                    \
  class tag##Node FINAL : public NullaryNode {                                \
    tag##Node(const tag##Node&) = delete;                                     \
    tag##Node& operator=(const tag##Node&) = delete;                          \
    virtual void forceCompilation();                                          \
                                                                              \
   public:                                                                    \
    explicit tag##Node(SymbolTable& Symtab) : NullaryNode(Symtab, Op##tag) {} \
    ~tag##Node() OVERRIDE {}                                                  \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; }    \
    NODE_DECLS                                                                \
  };
AST_NULLARYNODE_TABLE
#undef X

class StreamNode : public NullaryNode {
  StreamNode() = delete;
  StreamNode(const StreamNode&) = delete;
  StreamNode& operator=(const StreamNode&) = delete;

  static constexpr uint8_t EncodingShift = 4;
  static constexpr uint8_t EncodingLimit = uint8_t(1) << EncodingShift;
  static constexpr uint8_t EncodingMask = EncodingLimit - 1;

 public:
  StreamNode(SymbolTable& Symtab,
             decode::StreamKind StrmKind,
             decode::StreamType StrmType)
      : NullaryNode(Symtab, OpStream), StrmKind(StrmKind), StrmType(StrmType) {}
  ~StreamNode() OVERRIDE {}

  decode::StreamKind getStreamKind() const { return StrmKind; }
  decode::StreamType getStreamType() const { return StrmType; }

  uint8_t getEncoding() const { return encode(StrmKind, StrmType); }

  static uint8_t encode(decode::StreamKind Kind, decode::StreamType Type) {
    assert(int(Kind) < EncodingLimit);
    assert(int(Type) < EncodingLimit);
    uint8_t Encoding = (uint8_t(Kind) << EncodingShift) | uint8_t(Type);
    return Encoding;
  }

  static void decode(uint8_t Encoding,
                     decode::StreamKind& Kind,
                     decode::StreamType& Type) {
    Kind = decode::StreamKind(Encoding >> EncodingShift);
    Type = decode::StreamType(Encoding & EncodingMask);
  }

  static bool implementsClass(NodeType Type) { return OpStream; }

 protected:
  decode::StreamKind StrmKind;
  decode::StreamType StrmType;
};

class IntegerNode : public NullaryNode {
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode& operator=(const IntegerNode&) = delete;
  IntegerNode() = delete;

 public:
  ~IntegerNode() OVERRIDE {}
  decode::ValueFormat getFormat() const { return Format; }
  decode::IntType getValue() const { return Value; }
  static bool implementsClass(NodeType Type);
  bool isDefaultValue() const { return isDefault; }

 protected:
  decode::IntType Value;
  decode::ValueFormat Format;
  bool isDefault;
  // Note: ValueFormat provided so that we can echo back out same
  // representation as when lexing s-expressions.
  IntegerNode(SymbolTable& Symtab,
              NodeType Type,
              decode::IntType Value,
              decode::ValueFormat Format,
              bool isDefault = false)
      : NullaryNode(Symtab, Type),
        Value(Value),
        Format(Format),
        isDefault(isDefault) {}
};

#define X(tag, format, defval, mergable, NODE_DECLS)                       \
  class tag##Node FINAL : public IntegerNode {                             \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    tag##Node() = delete;                                                  \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab,                                         \
              decode::IntType Value,                                       \
              decode::ValueFormat Format = decode::ValueFormat::Decimal)   \
        : IntegerNode(Symtab, Op##tag, Value, Format, false) {}            \
    tag##Node(SymbolTable& Symtab)                                         \
        : IntegerNode(Symtab,                                              \
                      Op##tag,                                             \
                      (defval),                                            \
                      decode::ValueFormat::Decimal,                        \
                      true) {}                                             \
    ~tag##Node() OVERRIDE {}                                               \
                                                                           \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; } \
    NODE_DECLS                                                             \
  };
AST_INTEGERNODE_TABLE
#undef X

class SymbolNode FINAL : public NullaryNode {
  SymbolNode(const SymbolNode&) = delete;
  SymbolNode& operator=(const SymbolNode&) = delete;
  SymbolNode() = delete;
  friend class SymbolTable;

 public:
  SymbolNode(SymbolTable& Symtab, const std::string& Name)
      : NullaryNode(Symtab, OpSymbol), Name(Name) {
    init();
  }
  ~SymbolNode() OVERRIDE {}
  const std::string& getName() const { return Name; }
  const Node* getDefineDefinition() const { return DefineDefinition; }
  void setDefineDefinition(Node* Defn) { DefineDefinition = Defn; }

  PredefinedSymbol getPredefinedSymbol() const { return PredefinedValue; }

  static bool implementsClass(NodeType Type) { return Type == OpSymbol; }

 private:
  std::string Name;
  Node* DefineDefinition;
  PredefinedSymbol PredefinedValue;
  void init() {
    DefineDefinition = nullptr;
    PredefinedValue = PredefinedSymbol::Unknown;
  }
  void clearCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void setPredefinedSymbol(PredefinedSymbol NewValue);
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
  UnaryNode(SymbolTable& Symtab, NodeType Type, Node* Kid)
      : Node(Symtab, Type) {
    Kids[0] = Kid;
  }
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public UnaryNode {                               \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid)                              \
        : UnaryNode(Symtab, Op##tag, Kid) {}                               \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
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
  BinaryNode(SymbolTable& Symtab, NodeType Type, Node* Kid1, Node* Kid2)
      : Node(Symtab, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
  }
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public BinaryNode {                              \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2)                 \
        : BinaryNode(Symtab, Op##tag, Kid1, Kid2) {}                       \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
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
  TernaryNode(SymbolTable& Symtab,
              NodeType Type,
              Node* Kid1,
              Node* Kid2,
              Node* Kid3)
      : Node(Symtab, Type) {
    Kids[0] = Kid1;
    Kids[1] = Kid2;
    Kids[2] = Kid3;
  }
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public TernaryNode {                             \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2, Node* Kid3)     \
        : TernaryNode(Symtab, Op##tag, Kid1, Kid2, Kid3) {}                \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_TERNARYNODE_TABLE
#undef X

class NaryNode : public Node {
  NaryNode(const NaryNode&) = delete;
  NaryNode& operator=(const NaryNode&) = delete;

 public:
  int getNumKids() const OVERRIDE FINAL { return Kids.size(); }

  Node* getKid(int Index) const OVERRIDE FINAL { return Kids[Index]; }

  void setKid(int Index, Node* N) OVERRIDE FINAL { Kids[Index] = N; }

  void append(Node* Kid) FINAL { Kids.emplace_back(Kid); }
  ~NaryNode() OVERRIDE {}

  static bool implementsClass(NodeType Type);

 protected:
  std::vector<Node*> Kids;
  NaryNode(SymbolTable& Symtab, NodeType Type)
      : Node(Symtab, Type) {}
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public NaryNode {                                \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    explicit tag##Node(SymbolTable& Symtab) : NaryNode(Symtab, Op##tag) {} \
    ~tag##Node() OVERRIDE {}                                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_NARYNODE_TABLE
#undef X

class SelectBaseNode : public NaryNode {
  SelectBaseNode(const SelectBaseNode&) = delete;
  SelectBaseNode& operator=(const SelectBaseNode&) = delete;

 public:
  void clearCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  const CaseNode* getCase(decode::IntType Key) const;

 protected:
  // TODO(karlschimpf) Hook this up to an allocator?
  std::unordered_map<decode::IntType, const CaseNode*> LookupMap;

  SelectBaseNode(SymbolTable& Symtab, NodeType Type) : NaryNode(Symtab, Type) {}
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public SelectBaseNode {                          \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
    virtual void forceCompilation();                                       \
                                                                           \
   public:                                                                 \
    explicit tag##Node(SymbolTable& Symtab)                                \
        : SelectBaseNode(Symtab, Op##tag) {}                               \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_SELECTNODE_TABLE
#undef X

class OpcodeNode FINAL : public SelectBaseNode {
  OpcodeNode(const OpcodeNode&) = delete;
  OpcodeNode& operator=(const OpcodeNode&) = delete;

 public:
  explicit OpcodeNode(SymbolTable& Symtab) : SelectBaseNode(Symtab, OpOpcode) {}
  static bool implementsClass(NodeType Type) { return OpOpcode == Type; }
  const CaseNode* getWriteCase(decode::IntType Value,
                               uint32_t& SelShift,
                               decode::IntType& CaseMask) const;

 private:
  // Associates Opcode Case's with range [Min, Max].
  //
  // Note: Code assumes that shift value and case key is implicitly
  // encoded into range [Min, Max]. Therefore, they are not used in
  // comparison.
  class WriteRange {
    WriteRange() = delete;

   public:
    WriteRange(const CaseNode* Case,
               decode::IntType Min,
               decode::IntType Max,
               uint32_t ShiftValue)
        : Case(Case), Min(Min), Max(Max), ShiftValue(ShiftValue) {}
    ~WriteRange() {}
    const CaseNode* getCase() const { return Case; }
    decode::IntType getMin() const { return Min; }
    decode::IntType getMax() const { return Max; }
    uint32_t getShiftValue() const { return ShiftValue; }
    int compare(const WriteRange& R) const;
    bool operator<(const WriteRange& R) const { return compare(R) < 0; }
    void trace() const {
      TRACE_BLOCK({ traceInternal(""); });
    }
    void trace(const char* Prefix) const {
      TRACE_BLOCK({ traceInternal(Prefix); });
    }
    TraceClassSexp& getTrace() const { return Case->getTrace(); }

   private:
    const CaseNode* Case;
    decode::IntType Min;
    decode::IntType Max;
    uint32_t ShiftValue;
    void traceInternal(const char* Prefix) const;
  };
  void clearCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  typedef std::vector<WriteRange> CaseRangeVectorType;
  CaseRangeVectorType CaseRangeVector;
  void installCaseRanges();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_AST_H
