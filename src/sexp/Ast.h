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

#ifndef DECOMPRESSOR_SRC_SEXP_AST_H_
#define DECOMPRESSOR_SRC_SEXP_AST_H_

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "interp/IntFormats.h"
#include "sexp/Ast.def"
#include "sexp/NodeType.h"
#include "sexp/PredefinedStrings.def"
#include "stream/ValueFormat.h"

namespace wasm {

namespace utils {
class TraceClass;
}  // end of namespace utils

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
  IntegerValue& operator=(const IntegerValue&) = delete;

 public:
  IntegerValue();
  explicit IntegerValue(decode::IntType Value, decode::ValueFormat Format);
  IntegerValue(NodeType Type,
               decode::IntType Value,
               decode::ValueFormat Format,
               bool isDefault = false);
  explicit IntegerValue(const IntegerValue& V);
  ~IntegerValue();

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

PredefinedSymbol toPredefinedSymbol(uint32_t Value);
const char* getName(PredefinedSymbol);

// TODO(karlschimpf): Code no longer uses allocator. Remove from API.
class SymbolTable FINAL : public std::enable_shared_from_this<SymbolTable> {
  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;

 public:
  // Use std::make_shared() to build.
  explicit SymbolTable();
  explicit SymbolTable(std::shared_ptr<SymbolTable> EnclosingScope);
  ~SymbolTable();
  // Gets existing symbol if known. Otherwise returns nullptr.
  SymbolNode* getSymbol(const std::string& Name);
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
  const Node* getInstalledRoot() const { return Root; }
  const FileHeaderNode* getSourceHeader() const;
  const FileHeaderNode* getTargetHeader() const { return TargetHeader; }
  void clear();
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

  // Returns the cached value associated with a node, or nullptr if not cached.
  Node* getCachedValue(const Node* Nd) const { return CachedValue[Nd]; }
  void setCachedValue(const Node* Nd, Node* Value) { CachedValue[Nd] = Value; }

  // Strips all callback actions from the algorithm, except for the names
  // specified. Returns the updated tree.
  void stripCallbacksExcept(std::set<std::string>& KeepActions);

  // Strips out literal definitions and replaces literal uses.
  void stripLiterals();

  void setTraceProgress(bool NewValue);
  virtual void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();

  // For debugging.
  utils::TraceClass& getTrace();
  void describe(FILE* Out);

 private:
  typedef std::map<const Node*, Node*> CachedValueMap;
  std::shared_ptr<SymbolTable> EnclosingScope;
  std::vector<Node*> Allocated;
  std::shared_ptr<utils::TraceClass> Trace;
  Node* Root;
  const FileHeaderNode* TargetHeader;
  Node* Error;
  int NextCreationIndex;
  std::map<std::string, SymbolNode*> SymbolMap;
  std::map<IntegerValue, IntegerNode*> IntMap;
  std::vector<SymbolNode*> Predefined;
  CallbackNode* BlockEnterCallback;
  CallbackNode* BlockExitCallback;
  mutable CachedValueMap CachedValue;

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
  Node* stripCallbacksExcept(std::set<std::string>& KeepActions, Node* Root);
  Node* stripLiteralUses(Node* Root);
  Node* stripLiteralDefs(Node* Root);
};

class Node {
  Node() = delete;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  friend class SymbolTable;

 public:
  typedef size_t IndexType;
  class Iterator {
    Iterator() = delete;

   public:
    explicit Iterator(const Node* Nd, int Index);
    Iterator(const Iterator& Iter);
    Iterator& operator=(const Iterator& Iter);
    void operator++() { ++Index; }
    void operator--() { --Index; }
    bool operator==(const Iterator& Iter);
    bool operator!=(const Iterator& Iter);
    Node* operator*() const;

   private:
    const Node* Nd;
    int Index;
  };

  virtual ~Node();

  SymbolTable& getSymtab() { return Symtab; }
  NodeType getRtClassId() const { return Type; }
  NodeType getType() const { return Type; }
  const char* getName() const;
  const char* getNodeName() const;
  utils::TraceClass& getTrace() const;
  bool hasKids() const { return getNumKids() > 0; }

  // General API to children.
  virtual int getNumKids() const = 0;
  virtual Node* getKid(int Index) const = 0;
  virtual void setKid(int Index, Node* N) = 0;
  void setLastKid(Node* N);
  Node* getLastKid() const;

  // Counts number of nodes in tree defined by this.
  size_t getTreeSize() const;

  // Validates the node, based on the parents.
  virtual bool validateNode(NodeVectorType& Parents);

  // Recursively walks tree and validates (Scope allows lexical validation).
  // Returns true if validation succeeds.
  bool validateKid(NodeVectorType& Parents, Node* Kid);
  bool validateKids(NodeVectorType& Parents);
  bool validateSubtree(NodeVectorType& Parents);

  // WARNING: Only supported if underlying type allows.
  virtual void append(Node* Kid);

  int getCreationIndex() const { return CreationIndex; }

  // General iterators for walking kids.
  Iterator begin() const;
  Iterator end() const;
  Iterator rbegin() const;
  Iterator rend() const;

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
  Node(SymbolTable& Symtab, NodeType Type);
  virtual void clearCaches(NodeVectorType& AdditionalNodes);
  virtual void installCaches(NodeVectorType& AdditionalNodes);
};

class NullaryNode : public Node {
  NullaryNode() = delete;
  NullaryNode(const NullaryNode&) = delete;
  NullaryNode& operator=(const NullaryNode&) = delete;

 public:
  ~NullaryNode() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  NullaryNode(SymbolTable& Symtab, NodeType Type);
};

class UnaryNode : public Node {
  UnaryNode() = delete;
  UnaryNode(const UnaryNode&) = delete;
  UnaryNode& operator=(const UnaryNode&) = delete;

 public:
  ~UnaryNode() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[1];
  UnaryNode(SymbolTable& Symtab, NodeType Type, Node* Kid);
};

class BinaryNode : public Node {
  BinaryNode() = delete;
  BinaryNode(const BinaryNode&) = delete;
  BinaryNode& operator=(const BinaryNode&) = delete;

 public:
  ~BinaryNode() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[2];
  BinaryNode(SymbolTable& Symtab, NodeType Type, Node* Kid1, Node* Kid2);
};

class TernaryNode : public Node {
  TernaryNode() = delete;
  TernaryNode(const TernaryNode&) = delete;
  TernaryNode& operator=(const TernaryNode&) = delete;

 public:
  ~TernaryNode() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[3];
  TernaryNode(SymbolTable& Symtab,
              NodeType Type,
              Node* Kid1,
              Node* Kid2,
              Node* Kid3);
};

class NaryNode : public Node {
  NaryNode() = delete;
  NaryNode(const NaryNode&) = delete;
  NaryNode& operator=(const NaryNode&) = delete;

 public:
  ~NaryNode() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;
  void append(Node* Kid) FINAL OVERRIDE;
  void clearKids();

  static bool implementsClass(NodeType Type);

 protected:
  std::vector<Node*> Kids;
  NaryNode(SymbolTable& Symtab, NodeType Type);
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public NullaryNode {                             \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    explicit tag##Node(SymbolTable& Symtab);                               \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; } \
    NODE_DECLS                                                             \
  };
AST_NULLARYNODE_TABLE
#undef X

class IntegerNode : public NullaryNode {
  IntegerNode() = delete;
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode& operator=(const IntegerNode&) = delete;

 public:
  ~IntegerNode() OVERRIDE;
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
              bool isDefault = false);
};

#define X(tag, format, defval, mergable, NODE_DECLS)                       \
  class tag##Node FINAL : public IntegerNode {                             \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab,                                         \
              decode::IntType Value,                                       \
              decode::ValueFormat Format = decode::ValueFormat::Decimal);  \
                                                                           \
    tag##Node(SymbolTable& Symtab);                                        \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Type == Op##tag; } \
    NODE_DECLS                                                             \
  };
AST_INTEGERNODE_TABLE
#undef X

// The Value/Numbit fields are set by validateNode(). NumBits is the number of
// bits used to reach this accept, and Value encodes the path (from leaf to
// root) for the accept node. Note: The Value is unique for each accept, and
// therefore is used as the (case) selector value.
class BinaryAcceptNode FINAL : public IntegerNode {
  BinaryAcceptNode() = delete;
  BinaryAcceptNode(const BinaryAcceptNode&) = delete;
  BinaryAcceptNode& operator=(const BinaryAcceptNode&) = delete;

 public:
  BinaryAcceptNode(SymbolTable& Symtab);
  BinaryAcceptNode(SymbolTable& Symtab,
                   decode::IntType Value,
                   unsigned NumBits);
  ~BinaryAcceptNode() OVERRIDE;
  bool validateNode(NodeVectorType& Parents) OVERRIDE;
  unsigned getNumBits() const { return NumBits; }

  static bool implementsClass(NodeType Type) { return Type == OpBinaryAccept; }

 protected:
  unsigned NumBits;
};

// Holds cached information about a Symbol
class SymbolDefnNode FINAL : public NullaryNode {
  SymbolDefnNode() = delete;
  SymbolDefnNode(const SymbolDefnNode&) = delete;
  SymbolDefnNode& operator=(const SymbolDefnNode&) = delete;
public:
  SymbolDefnNode(SymbolTable& Symtab);
  const SymbolNode* getSymbol() const { return Symbol; }
  void setSymbol(const SymbolNode* Nd) { Symbol = Nd; }
  const std::string& getName() const;
  const Node* getDefineDefinition() { return DefineDefinition; }
  void setDefineDefinition(const Node* Defn) { DefineDefinition = Defn; }
  const Node* getLiteralDefinition() const { return LiteralDefinition; }
  void setLiteralDefinition(const Node* Defn) { LiteralDefinition = Defn; }

  static bool implementsClass(NodeType Type) { return Type == OpSymbolDefn; }

private:
  const SymbolNode* Symbol;
  const Node* DefineDefinition;
  const Node* LiteralDefinition;
};

class SymbolNode FINAL : public NullaryNode {
  SymbolNode() = delete;
  SymbolNode(const SymbolNode&) = delete;
  SymbolNode& operator=(const SymbolNode&) = delete;
  friend class SymbolTable;

 public:
  SymbolNode(SymbolTable& Symtab, const std::string& Name);
  ~SymbolNode() OVERRIDE;
  const std::string& getName() const { return Name; }
  const Node* getDefineDefinition() const { return DefineDefinition; }
  void setDefineDefinition(Node* Defn) { DefineDefinition = Defn; }
#if 0
  const Node* getLiteralDefinition() const { return LiteralDefinition; }
  void setLiteralDefinition(Node* Defn) { LiteralDefinition = Defn; }
#else
  const Node* getLiteralDefinition() const {
    return getSymbolDefn()->getLiteralDefinition();
  }
  void setLiteralDefinition(Node* Defn) {
    getSymbolDefn()->setLiteralDefinition(Defn);
  }
#endif
  PredefinedSymbol getPredefinedSymbol() const { return PredefinedValue; }
  static bool implementsClass(NodeType Type) { return Type == OpSymbol; }

 private:
  std::string Name;
  Node* DefineDefinition;
#if 0
  Node* LiteralDefinition;
#endif
  PredefinedSymbol PredefinedValue;
  void init();
  SymbolDefnNode* getSymbolDefn() const;
  void clearCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void setPredefinedSymbol(PredefinedSymbol NewValue);
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public UnaryNode {                               \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid);                             \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_UNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public BinaryNode {                              \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2);                \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_BINARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public TernaryNode {                             \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2, Node* Kid3);    \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_TERNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public NaryNode {                                \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    explicit tag##Node(SymbolTable& Symtab);                               \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_NARYNODE_TABLE
#undef X

class SelectBaseNode : public NaryNode {
  SelectBaseNode() = delete;
  SelectBaseNode(const SelectBaseNode&) = delete;
  SelectBaseNode& operator=(const SelectBaseNode&) = delete;

 public:
  ~SelectBaseNode() OVERRIDE;
  void clearCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  void installCaches(NodeVectorType& AdditionalNodes) OVERRIDE;
  const CaseNode* getCase(decode::IntType Key) const;

 protected:
  // TODO(karlschimpf) Hook this up to an allocator?
  std::unordered_map<decode::IntType, const CaseNode*> LookupMap;

  SelectBaseNode(SymbolTable& Symtab, NodeType Type);
};

#define X(tag, NODE_DECLS)                                                 \
  class tag##Node FINAL : public SelectBaseNode {                          \
    tag##Node() = delete;                                                  \
    tag##Node(const tag##Node&) = delete;                                  \
    tag##Node& operator=(const tag##Node&) = delete;                       \
                                                                           \
   public:                                                                 \
    explicit tag##Node(SymbolTable& Symtab);                               \
    ~tag##Node() OVERRIDE;                                                 \
    static bool implementsClass(NodeType Type) { return Op##tag == Type; } \
    NODE_DECLS                                                             \
  };
AST_SELECTNODE_TABLE
#undef X

class OpcodeNode FINAL : public SelectBaseNode {
  OpcodeNode() = delete;
  OpcodeNode(const OpcodeNode&) = delete;
  OpcodeNode& operator=(const OpcodeNode&) = delete;

 public:
  explicit OpcodeNode(SymbolTable& Symtab);
  ~OpcodeNode() OVERRIDE;
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
   public:
    // Do not use this form, unless assigned to. Initializes with undefined
    // range.
    WriteRange();
    WriteRange(const CaseNode* Case,
               decode::IntType Min,
               decode::IntType Max,
               uint32_t ShiftValue);
    WriteRange(const WriteRange& R);
    void assign(const WriteRange& R);
    WriteRange& operator=(const WriteRange& R) {
      assign(R);
      return *this;
    }
    ~WriteRange();
    const CaseNode* getCase() const { return Case; }
    decode::IntType getMin() const { return Min; }
    decode::IntType getMax() const { return Max; }
    uint32_t getShiftValue() const { return ShiftValue; }
    int compare(const WriteRange& R) const;
    bool operator<(const WriteRange& R) const { return compare(R) < 0; }
    utils::TraceClass& getTrace() const;

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

class BinaryEvalNode : public UnaryNode {
  BinaryEvalNode() = delete;
  BinaryEvalNode(const BinaryEvalNode&) = delete;
  BinaryEvalNode& operator=(const BinaryEvalNode&) = delete;

 public:
  explicit BinaryEvalNode(SymbolTable& Symtab, Node* Encoding);
  ~BinaryEvalNode() OVERRIDE;
  bool validateNode(NodeVectorType& Parents) OVERRIDE;

  const Node* getEncoding(decode::IntType Value) const;
  bool addEncoding(BinaryAcceptNode* Encoding);

  static bool implementsClass(NodeType Type) { return OpBinaryEval == Type; }

 private:
  std::unordered_map<decode::IntType, const Node*> LookupMap;
  Node* NotFound;
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_AST_H_
