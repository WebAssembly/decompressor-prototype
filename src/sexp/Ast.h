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

#include "interp/IntFormats.h"
#include "sexp/Ast.def"
#include "sexp/NodeType.h"
#include "sexp/Strings.def"
#include "stream/WriteUtils.h"
#include "utils/Casting.h"
#include "utils/Defs.h"
#include "utils/initialized_ptr.h"
#include "utils/Trace.h"

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wasm {

namespace filt {

class BinaryAcceptNode;
class FileHeaderNode;
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
    return (*Predefined)[uint32_t(Sym)];
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
  const Node* getInstalledRoot() const { return Root; }
  const FileHeaderNode* getSourceHeader() const;
  const FileHeaderNode* getTargetHeader() const { return TargetHeader; }
  void clear() { SymbolMap.clear(); }
  int getNextCreationIndex() { return ++NextCreationIndex; }

  template <typename T>
  T* create();
  template <typename T>
  T* create(Node* Nd);
  template <typename T>
  T* create(Node* Nd1, Node* Nd2);
  template <typename T>
  T* create(Node* Nd1, Node* Nd2, Node* Nd3);
  BinaryAcceptNode* createBinaryAccept(decode::IntType Value, unsigned NumBits);

  // Strips all callback actions from the algorithm, except for the names
  // specified. Returns the updated tree.
  void stripCallbacksExcept(std::vector<std::string>& KeepActions) {
    install(stripCallbacksExcept(KeepActions, Root));
  }

  // Strips out literal definitions and replaces literal uses.
  void stripLiterals();

  void setTraceProgress(bool NewValue);
  virtual void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }

 private:
  std::vector<Node*>* Allocated;
  std::shared_ptr<utils::TraceClass> Trace;
  Node* Root;
  const FileHeaderNode* TargetHeader;
  Node* Error;
  int NextCreationIndex;
  std::map<std::string, SymbolNode*> SymbolMap;
  std::map<IntegerValue, IntegerNode*> IntMap;
  // TODO(karlschimpf) Figure out why vector destroy not working.
  std::vector<SymbolNode*>* Predefined;
  CallbackNode* BlockEnterCallback;
  CallbackNode* BlockExitCallback;

  void init();
  void deallocateNodes();

  void installDefinitions(Node* Root);
  void clearSubtreeCaches(Node* Nd,
                          VisitedNodesType& VisitedNodes,
                          NodeVectorType& AdditionalNodes);
  void installSubtreeCaches(Node* Nd,
                            VisitedNodesType& VisitedNoes,
                            NodeVectorType& AdditionalNodes);

  Node* stripUsing(Node* Root, std::function<Node*(Node*)> stripKid);
  Node* stripCallbacksExcept(std::vector<std::string>& KeepActions, Node* Root);
  Node* stripLiteralUses(Node* Root);
  Node* stripLiteralDefs(Node* Root);
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

  utils::TraceClass& getTrace() const { return Symtab.getTrace(); }

  bool hasKids() const { return getNumKids() > 0; }

  // General API to children.
  virtual int getNumKids() const = 0;

  virtual Node* getKid(int Index) const = 0;

  virtual void setKid(int Index, Node* N) = 0;

  // Counts number of nodes in tree defined by this.
  size_t getTreeSize() const;

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

  // Returns true if the node defines an AST node that corresponds to an
  // integer format (i.e. IntTypeFormat);
  bool definesIntTypeFormat() const;
  // Gets the corresponding format if definesIntTypeFormat() returns true.
  interp::IntTypeFormat getIntTypeFormat() const;

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

// The Value/Numbit fields are set by validateNode(). NumBits is the number of
// bits used to reach this accept, and Value encodes the path (from leaf to
// root) for the accept node. Note: The Value is unique for each accept, and
// therefore is used as the (case) selector value.
class BinaryAcceptNode : public IntegerNode {
  BinaryAcceptNode(const BinaryAcceptNode&) = delete;
  BinaryAcceptNode& operator=(const BinaryAcceptNode&) = delete;
  BinaryAcceptNode() = delete;

 public:
  BinaryAcceptNode(SymbolTable& Symtab)
      : IntegerNode(Symtab,
                    OpBinaryAccept,
                    0,
                    decode::ValueFormat::Hexidecimal,
                    true),
        NumBits(0) {}
  BinaryAcceptNode(SymbolTable& Symtab, decode::IntType Value, unsigned NumBits)
      : IntegerNode(Symtab,
                    OpBinaryAccept,
                    Value,
                    decode::ValueFormat::Hexidecimal,
                    false),
        NumBits(NumBits) {}
  ~BinaryAcceptNode() OVERRIDE {}
  bool validateNode(NodeVectorType& Parents) OVERRIDE;
  unsigned getNumBits() const { return NumBits; }

  static bool implementsClass(NodeType Type) { return Type == OpBinaryAccept; }

 protected:
  unsigned NumBits;
};

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
  const Node* getLiteralDefinition() const { return LiteralDefinition; }
  void setLiteralDefinition(Node* Defn) { LiteralDefinition = Defn; }

  PredefinedSymbol getPredefinedSymbol() const { return PredefinedValue; }

  static bool implementsClass(NodeType Type) { return Type == OpSymbol; }

 private:
  std::string Name;
  Node* DefineDefinition;
  Node* LiteralDefinition;
  PredefinedSymbol PredefinedValue;
  void init() {
    DefineDefinition = nullptr;
    LiteralDefinition = nullptr;
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

  void append(Node* Kid) FINAL OVERRIDE;
  void clearKids() { Kids.clear(); }
  ~NaryNode() OVERRIDE {}

  static bool implementsClass(NodeType Type);

 protected:
  std::vector<Node*> Kids;
  NaryNode(SymbolTable& Symtab, NodeType Type) : Node(Symtab, Type) {}
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
    utils::TraceClass& getTrace() const { return Case->getTrace(); }

   private:
    const CaseNode* Case;
    decode::IntType Min;
    decode::IntType Max;
    uint32_t ShiftValue;
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
