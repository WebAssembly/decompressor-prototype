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
#include "sexp/Ast-defs.h"
#include "sexp/NodeType.h"
#include "sexp/PredefinedStrings-defs.h"
#include "stream/ValueFormat.h"

namespace wasm {

namespace utils {
class TraceClass;
}  // end of namespace utils

namespace filt {

// Note: mutable fields define caching values in classes.

class BinaryAccept;
class Define;
class Algorithm;
class Header;
class IntegerNode;
class LiteralDef;
class LiteralActionDef;
class Node;
class ReadHeader;
class SourceHeader;
class SymbolDefn;
class Symbol;
class SymbolTable;
class Callback;
class WriteHeader;

#define X(NAME, FORMAT, DEFAULT, MERGE, BASE, DECLS, INIT) class NAME;
AST_INTEGERNODE_TABLE
#undef X

typedef std::unordered_set<Node*> VisitedNodesType;
typedef std::vector<Node*> NodeVectorType;
typedef std::vector<const Node*> ConstNodeVectorType;

// extern AstTraitsType AstTraits[NumNodeTypes];

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

// Defines constants for predefined symbols (i.e. instances of Symbol
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
charstring getName(PredefinedSymbol);

class CallFrame {
  CallFrame(const CallFrame&) = delete;
  CallFrame& operator=(const CallFrame&) = delete;

 public:
  explicit CallFrame(const Define* Def);
  ~CallFrame();
  bool isConsistent() { return InitSuccessful; }
  size_t getNumArgs() const { return ParamTypes.size(); }
  size_t getNumLocals() const { return NumLocals; }
  size_t getNumParams() const { return NumParams; }
  const Node* getParam(size_t Index);

 private:
  std::vector<const Node*> ParamTypes;
  mutable size_t NumParams;
  mutable size_t NumLocals;
  bool InitSuccessful;
  void init(const Define* Def);
};

// TODO(karlschimpf): Code no longer uses allocator. Remove from API.
class SymbolTable FINAL : public std::enable_shared_from_this<SymbolTable> {
  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;

 public:
  typedef std::shared_ptr<SymbolTable> SharedPtr;
  typedef std::unordered_set<const Symbol*> SymbolSet;
  typedef std::unordered_set<const IntegerNode*> ActionValueSet;
  typedef std::unordered_set<const LiteralActionDef*> ActionDefSet;
  // Use std::make_shared() to build.
  explicit SymbolTable();
  explicit SymbolTable(std::shared_ptr<SymbolTable> EnclosingScope);
  ~SymbolTable();
  SharedPtr getEnclosingScope() { return EnclosingScope; }
  void setEnclosingScope(SharedPtr Symtab);
  // Gets existing symbol if known. Otherwise returns nullptr.
  Symbol* getSymbol(const std::string& Name);
  // Returns the corresponding symbol for the predefined symbol (creates if
  // necessary).
  Symbol* getPredefined(PredefinedSymbol Sym);
  // Gets existing symbol if known. Otherwise returns newly created symbol.
  // Used to keep symbols unique within filter s-expressions.
  Symbol* getOrCreateSymbol(const std::string& Name);
  // Returns local version of symbol definitions associated with the
  // symbol. Used to get local cached symbol definitions when interpreting
  // nodes with a symbol lookup, such as Eval.
  SymbolDefn* getSymbolDefn(const Symbol* Symbol);

  void collectActionDefs(ActionDefSet& DefSet);

  // Gets integer node (as defined by the arguments) if known. Otherwise
  // returns newly created integer.

  // Gets actions corresponding to enter/exit block.
  const Callback* getBlockEnterCallback();
  const Callback* getBlockExitCallback();
  const Algorithm* getAlgorithm() const { return Alg; }
  void setAlgorithm(const Algorithm* Alg);
  // Install current algorithm
  bool install();
  bool isAlgorithmInstalled() const { return IsAlgInstalled; }
  Node* getError() const { return Err; }
  const Header* getSourceHeader() const;
  const Header* getReadHeader() const;
  const Header* getWriteHeader() const;

  // True if the algorithm specifies how to read an algorithm
  // (i.e. the source and target headers are the same).
  bool specifiesAlgorithm() const;
  void clearSymbols();
  int getNextCreationIndex() { return ++NextCreationIndex; }

  template <typename T>
  T* create();
  template <typename T>
  T* create(Node* Nd);
  template <typename T>
  T* create(Node* Nd1, Node* Nd2);
  template <typename T>
  T* create(Node* Nd1, Node* Nd2, Node* Nd3);
  template <class T>
  T* create(decode::IntType Value, decode::ValueFormat Format);

  BinaryAccept* createBinaryAccept(decode::IntType Value, unsigned NumBits);

  // Returns the cached value associated with a node, or nullptr if not cached.
  Node* getCachedValue(const Node* Nd) { return CachedValue[Nd]; }
  void setCachedValue(const Node* Nd, Node* Value) { CachedValue[Nd] = Value; }

  // Adds the given callback literal to the set of known callback literals.
  void insertCallbackLiteral(const LiteralActionDef* Defn);
  void insertCallbackValue(const IntegerNode* IntNd);
  void insertUndefinedCallback(const Symbol* Sym) {
    UndefinedCallbacks.insert(Sym);
  }

  // Strips all callback actions from the algorithm, except for the names
  // specified. Returns the updated tree.
  void stripCallbacksExcept(std::set<std::string>& KeepActions);

  void stripSymbolicCallbacks();

  void stripLiteralUses();
  void stripLiteralDefs();
  // Strips out literal definitions and replaces literal uses.
  void stripLiterals();

  void setTraceProgress(bool NewValue);
  virtual void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();

  FILE* getErrorFile() const;
  // getErrorFile with error prefix added.
  FILE* error() const;

  // For debugging.
  utils::TraceClass& getTrace();
  void describe(FILE* Out, bool ShowInternalStructure = false);

 private:
  typedef std::map<const Node*, Node*> CachedValueMap;
  std::shared_ptr<SymbolTable> EnclosingScope;
  std::vector<Node*> Allocated;
  std::shared_ptr<utils::TraceClass> Trace;
  Algorithm* Alg;
  bool IsAlgInstalled;
  Node* Err;
  int NextCreationIndex;
  decode::IntType ActionBase;
  SymbolSet UndefinedCallbacks;
  ActionValueSet CallbackValues;
  ActionDefSet CallbackLiterals;
  std::map<std::string, Symbol*> SymbolMap;
  std::map<IntegerValue, IntegerNode*> IntMap;
  std::map<PredefinedSymbol, Symbol*> PredefinedMap;
  Callback* BlockEnterCallback;
  Callback* BlockExitCallback;
  CachedValueMap CachedValue;
  mutable const Header* CachedSourceHeader;
  mutable const Header* CachedReadHeader;
  mutable const Header* CachedWriteHeader;

  void init();
  void clearCaches();
  void deallocateNodes();

  void installPredefined();
  void installDefinitions(const Node* Root);

  bool areActionsConsistent();
  Node* stripUsing(Node* Root, std::function<Node*(Node*)> stripKid);
  Node* stripCallbacksExcept(std::set<std::string>& KeepActions, Node* Root);
  Node* stripSymbolicCallbackUses(Node* Root);
  Node* stripSymbolicCallbackDefs(Node* Root);
  Node* stripLiteralUses(Node* Root);
  void collectLiteralUseSymbols(SymbolSet& Symbols);
  Node* stripLiteralDefs(Node* Root, SymbolSet& DefSyms);
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
  bool isTextVisible() const;
  SymbolTable& getSymtab() const { return Symtab; }
  NodeType getRtClassId() const { return Type; }
  NodeType getType() const { return Type; }
  const char* getName() const;
  const char* getNodeName() const;
  utils::TraceClass& getTrace() const { return Symtab.getTrace(); }
  bool hasKids() const { return getNumKids() > 0; }

  // General API to children.
  virtual int getNumKids() const = 0;
  virtual Node* getKid(int Index) const = 0;
  virtual void setKid(int Index, Node* N) = 0;
  void setLastKid(Node* N);
  Node* getLastKid() const;

  // Following define (structural) compare.
  int compare(const Node* Nd) const;
  // Following only compares nodes, not kids.
  virtual int nodeCompare(const Node* Nd) const;
  // Returns comparison value for nodes that don't define a complete
  // structural compare.
  int compareIncomparable(const Node* Nd) const;

  // Counts number of nodes in tree defined by this.
  size_t getTreeSize() const;

  // Validates the node, based on the parents.
  virtual bool validateNode(ConstNodeVectorType& Parents) const;

  // Recursively walks tree and validates (Scope allows lexical validation).
  // Returns true if validation succeeds.
  bool validateKid(ConstNodeVectorType& Parents, const Node* Kid) const;
  bool validateKids(ConstNodeVectorType& Parents) const;
  bool validateSubtree(ConstNodeVectorType& Parents) const;

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

  FILE* getErrorFile() const { return Symtab.getErrorFile(); }
  FILE* error() const { return Symtab.error(); }

 protected:
  NodeType Type;
  SymbolTable& Symtab;
  int CreationIndex;
  Node(SymbolTable& Symtab, NodeType Type);
};

inline bool operator<(const Node& N1, const Node& N2) {
  return N1.compare(&N2) < 0;
}
inline bool operator<=(const Node& N1, const Node& N2) {
  return N1.compare(&N2) <= 0;
}
inline bool operator==(const Node& N1, const Node& N2) {
  return N1.compare(&N2) == 0;
}
inline bool operator!=(const Node& N1, const Node& N2) {
  return N1.compare(&N2) != 0;
}
inline bool operator>=(const Node& N1, const Node& N2) {
  return N1.compare(&N2) >= 0;
}
inline bool operator>(const Node& N1, const Node& N2) {
  return N1.compare(&N2) > 0;
}

class Nullary : public Node {
  Nullary() = delete;
  Nullary(const Nullary&) = delete;
  Nullary& operator=(const Nullary&) = delete;

 public:
  ~Nullary() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Nullary(SymbolTable& Symtab, NodeType Type);
};

// Base class for cached data. What these nodes have in common is that
// they are uncomparable because their content updates dynamically as
// data is cached. As a result, we use this base class to capture that
// concept.
class Cached : public Nullary {
  Cached() = delete;
  Cached(const Cached&) = delete;
  Cached& operator=(const Cached&) = delete;

 public:
  ~Cached() OVERRIDE;
  int nodeCompare(const Node* Nd) const OVERRIDE;

  static bool implementsClass(NodeType Type);

 protected:
  Cached(SymbolTable& Symtab, NodeType Type);
};

class IntegerNode : public Nullary {
  IntegerNode() = delete;
  IntegerNode(const IntegerNode&) = delete;
  IntegerNode& operator=(const IntegerNode&) = delete;

 public:
  ~IntegerNode() OVERRIDE;
  int nodeCompare(const Node* Nd) const OVERRIDE;
  decode::ValueFormat getFormat() const { return Value.Format; }
  decode::IntType getValue() const { return Value.Value; }
  bool isDefaultValue() const { return Value.isDefault; }

  static bool implementsClass(NodeType Type);

 protected:
  mutable IntegerValue Value;
  // Note: ValueFormat provided so that we can echo back out same
  // representation as when lexing s-expressions.
  IntegerNode(SymbolTable& Symtab,
              NodeType Type,
              decode::IntType Value,
              decode::ValueFormat Format,
              bool isDefault = false);
};

class Unary : public Node {
  Unary() = delete;
  Unary(const Unary&) = delete;
  Unary& operator=(const Unary&) = delete;

 public:
  ~Unary() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[1];
  Unary(SymbolTable& Symtab, NodeType Type, Node* Kid);
};

class Binary : public Node {
  Binary() = delete;
  Binary(const Binary&) = delete;
  Binary& operator=(const Binary&) = delete;

 public:
  ~Binary() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[2];
  Binary(SymbolTable& Symtab, NodeType Type, Node* Kid1, Node* Kid2);
};

class Ternary : public Node {
  Ternary() = delete;
  Ternary(const Ternary&) = delete;
  Ternary& operator=(const Ternary&) = delete;

 public:
  ~Ternary() OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;

  static bool implementsClass(NodeType Type);

 protected:
  Node* Kids[3];
  Ternary(SymbolTable& Symtab,
          NodeType Type,
          Node* Kid1,
          Node* Kid2,
          Node* Kid3);
};

class Nary : public Node {
  Nary() = delete;
  Nary(const Nary&) = delete;
  Nary& operator=(const Nary&) = delete;

 public:
  ~Nary() OVERRIDE;
  int nodeCompare(const Node*) const OVERRIDE;
  int getNumKids() const OVERRIDE FINAL;
  Node* getKid(int Index) const OVERRIDE FINAL;
  void setKid(int Index, Node* N) OVERRIDE FINAL;
  void append(Node* Kid) FINAL OVERRIDE;
  void clearKids();

  static bool implementsClass(NodeType Type);

 protected:
  std::vector<Node*> Kids;
  Nary(SymbolTable& Symtab, NodeType Type);
};

class Header : public Nary {
  Header() = delete;
  Header(const Header&) = delete;
  Header& operator=(const Header&) = delete;

 public:
  ~Header() OVERRIDE;
  static bool implementsClass(NodeType Type);

 protected:
  Header(SymbolTable& Symtab, NodeType Type);
};

class IntLookup FINAL : public Cached {
  IntLookup() = delete;
  IntLookup(const IntLookup&) = delete;
  IntLookup& operator=(const IntLookup&) = delete;

 public:
  typedef std::unordered_map<decode::IntType, const Node*> LookupMap;
  explicit IntLookup(SymbolTable&);
  ~IntLookup() OVERRIDE;
  const Node* get(decode::IntType Value);
  bool add(decode::IntType Value, const Node* Nd);

 private:
  LookupMap Lookup;
};

#define X(NAME, BASE, DECLS, INIT)               \
  class NAME FINAL : public BASE {               \
    NAME() = delete;                             \
    NAME(const NAME&) = delete;                  \
    NAME& operator=(const NAME&) = delete;       \
                                                 \
   public:                                       \
    explicit NAME(SymbolTable& Symtab);          \
    ~NAME() OVERRIDE;                            \
    static bool implementsClass(NodeType Type) { \
      return Type == NodeType::NAME;             \
    }                                            \
    DECLS                                        \
  };
AST_NULLARYNODE_TABLE
#undef X

#define X(NAME, FORMAT, DEFAULT, MERGE, BASE, DECLS, INIT)           \
  class NAME FINAL : public BASE {                                   \
    NAME() = delete;                                                 \
    NAME(const NAME&) = delete;                                      \
    NAME& operator=(const NAME&) = delete;                           \
                                                                     \
   public:                                                           \
    NAME(SymbolTable& Symtab,                                        \
         decode::IntType Value,                                      \
         decode::ValueFormat Format = decode::ValueFormat::Decimal); \
                                                                     \
    NAME(SymbolTable& Symtab);                                       \
    ~NAME() OVERRIDE;                                                \
    static bool implementsClass(NodeType Type) {                     \
      return Type == NodeType::NAME;                                 \
    }                                                                \
    DECLS                                                            \
  };
AST_INTEGERNODE_TABLE
#undef X

#define X(NAME, BASE, VALUE, FORMAT, DECLS, INIT) \
  class NAME FINAL : public BASE {                \
    NAME() = delete;                              \
    NAME(const NAME&) = delete;                   \
    NAME& operator=(const NAME&) = delete;        \
                                                  \
   public:                                        \
    NAME(SymbolTable& Symtab);                    \
    ~NAME() OVERRIDE;                             \
    static bool implementsClass(NodeType Type) {  \
      return Type == NodeType::NAME;              \
    }                                             \
    DECLS                                         \
  };
AST_LITERAL_TABLE
#undef X

// The Value/Numbit fields are set by validateNode(). NumBits is the number of
// bits used to reach this accept, and Value encodes the path (from leaf to
// root) for the accept node. Note: The Value is unique for each accept, and
// therefore is used as the (case) selector value.
class BinaryAccept FINAL : public IntegerNode {
  BinaryAccept() = delete;
  BinaryAccept(const BinaryAccept&) = delete;
  BinaryAccept& operator=(const BinaryAccept&) = delete;

 public:
  BinaryAccept(SymbolTable& Symtab);
  BinaryAccept(SymbolTable& Symtab, decode::IntType Value, unsigned NumBits);
  ~BinaryAccept() OVERRIDE;
  int nodeCompare(const Node*) const OVERRIDE;
  bool validateNode(ConstNodeVectorType& Parents) const OVERRIDE;
  unsigned getNumBits() const { return NumBits; }

  static bool implementsClass(NodeType Type) {
    return Type == NodeType::BinaryAccept;
  }

 protected:
  mutable unsigned NumBits;
};

// Holds cached information about a Symbol
class SymbolDefn FINAL : public Cached {
  SymbolDefn() = delete;
  SymbolDefn(const SymbolDefn&) = delete;
  SymbolDefn& operator=(const SymbolDefn&) = delete;

 public:
  SymbolDefn(SymbolTable& Symtab);
  ~SymbolDefn() OVERRIDE;
  const Symbol* getSymbol() const { return ForSymbol; }
  void setSymbol(const Symbol* Nd) { ForSymbol = Nd; }
  const std::string& getName() const;
  const Define* getDefineDefinition() const;
  void setDefineDefinition(const Define* Defn);
  const LiteralDef* getLiteralDefinition() const;
  void setLiteralDefinition(const LiteralDef* Defn);
  const LiteralActionDef* getLiteralActionDefinition() const;
  void setLiteralActionDefinition(const LiteralActionDef* Defn);

  static bool implementsClass(NodeType Type) {
    return Type == NodeType::SymbolDefn;
  }

 private:
  const Symbol* ForSymbol;
  mutable const Define* DefineDefinition;
  mutable const LiteralDef* LiteralDefinition;
  mutable const LiteralActionDef* LiteralActionDefinition;
};

class Symbol FINAL : public Nullary {
  Symbol() = delete;
  Symbol(const Symbol&) = delete;
  Symbol& operator=(const Symbol&) = delete;
  friend class SymbolTable;

 public:
  Symbol(SymbolTable& Symtab, const std::string& Name);
  ~Symbol() OVERRIDE;
  int nodeCompare(const Node* Nd) const OVERRIDE;
  const std::string& getName() const { return Name; }
  const Define* getDefineDefinition() const {
    return getSymbolDefn()->getDefineDefinition();
  }
  void setDefineDefinition(const Define* Defn) {
    getSymbolDefn()->setDefineDefinition(Defn);
  }
  const LiteralDef* getLiteralDefinition() const {
    return getSymbolDefn()->getLiteralDefinition();
  }
  void setLiteralDefinition(const LiteralDef* Defn) {
    getSymbolDefn()->setLiteralDefinition(Defn);
  }
  const LiteralActionDef* getLiteralActionDefinition() const {
    return getSymbolDefn()->getLiteralActionDefinition();
  }
  void setLiteralActionDefinition(const LiteralActionDef* Defn) {
    getSymbolDefn()->setLiteralActionDefinition(Defn);
  }
  PredefinedSymbol getPredefinedSymbol() const { return PredefinedValue; }
  bool isPredefinedSymbol() const { return PredefinedValueIsCached; }

  static bool implementsClass(NodeType Type) {
    return Type == NodeType::Symbol;
  }

 private:
  std::string Name;
  mutable PredefinedSymbol PredefinedValue;
  mutable bool PredefinedValueIsCached;
  void init();
  SymbolDefn* getSymbolDefn() const;
  void setPredefinedSymbol(PredefinedSymbol NewValue);
};

#define X(NAME, BASE, DECLS, INIT)               \
  class NAME FINAL : public BASE {               \
    NAME() = delete;                             \
    NAME(const NAME&) = delete;                  \
    NAME& operator=(const NAME&) = delete;       \
                                                 \
   public:                                       \
    NAME(SymbolTable& Symtab, Node* Kid);        \
    ~NAME() OVERRIDE;                            \
    static bool implementsClass(NodeType Type) { \
      return NodeType::NAME == Type;             \
    }                                            \
    DECLS                                        \
  };
AST_UNARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT)                     \
  class NAME FINAL : public BASE {                     \
    NAME() = delete;                                   \
    NAME(const NAME&) = delete;                        \
    NAME& operator=(const NAME&) = delete;             \
                                                       \
   public:                                             \
    NAME(SymbolTable& Symtab, Node* Kid1, Node* Kid2); \
    ~NAME() OVERRIDE;                                  \
    static bool implementsClass(NodeType Type) {       \
      return NodeType::NAME == Type;                   \
    }                                                  \
    DECLS                                              \
  };
AST_BINARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT)                                 \
  class NAME FINAL : public BASE {                                 \
    NAME() = delete;                                               \
    NAME(const NAME&) = delete;                                    \
    NAME& operator=(const NAME&) = delete;                         \
                                                                   \
   public:                                                         \
    NAME(SymbolTable& Symtab, Node* Kid1, Node* Kid2, Node* Kid3); \
    ~NAME() OVERRIDE;                                              \
    static bool implementsClass(NodeType Type) {                   \
      return NodeType::NAME == Type;                               \
    }                                                              \
    DECLS                                                          \
  };
AST_TERNARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT)               \
  class NAME FINAL : public BASE {               \
    NAME() = delete;                             \
    NAME(const NAME&) = delete;                  \
    NAME& operator=(const NAME&) = delete;       \
                                                 \
   public:                                       \
    explicit NAME(SymbolTable& Symtab);          \
    ~NAME() OVERRIDE;                            \
    static bool implementsClass(NodeType Type) { \
      return NodeType::NAME == Type;             \
    }                                            \
    DECLS                                        \
  };
AST_NARYNODE_TABLE
#undef X

class SelectBase : public Nary {
  SelectBase() = delete;
  SelectBase(const SelectBase&) = delete;
  SelectBase& operator=(const SelectBase&) = delete;

 public:
  ~SelectBase() OVERRIDE;
  const Case* getCase(decode::IntType Key) const;
  bool addCase(const Case* CaseNd) const;
  static bool implementsClass(NodeType Type);

 protected:
  SelectBase(SymbolTable& Symtab, NodeType Type);
  IntLookup* getIntLookup() const;
};

#define X(NAME, BASE, DECLS, INIT)               \
  class NAME FINAL : public BASE {               \
    NAME() = delete;                             \
    NAME(const NAME&) = delete;                  \
    NAME& operator=(const NAME&) = delete;       \
                                                 \
   public:                                       \
    explicit NAME(SymbolTable& Symtab);          \
    ~NAME() OVERRIDE;                            \
    static bool implementsClass(NodeType Type) { \
      return NodeType::NAME == Type;             \
    }                                            \
    DECLS                                        \
  };
AST_SELECTNODE_TABLE
#undef X

class Opcode FINAL : public SelectBase {
  Opcode() = delete;
  Opcode(const Opcode&) = delete;
  Opcode& operator=(const Opcode&) = delete;

 public:
  explicit Opcode(SymbolTable& Symtab);
  ~Opcode() OVERRIDE;
  static bool implementsClass(NodeType Type) {
    return NodeType::Opcode == Type;
  }
  const Case* getWriteCase(decode::IntType Value,
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
    WriteRange(const Case* C,
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
    const Case* getCase() const { return C; }
    decode::IntType getMin() const { return Min; }
    decode::IntType getMax() const { return Max; }
    uint32_t getShiftValue() const { return ShiftValue; }
    int compare(const WriteRange& R) const;
    bool operator<(const WriteRange& R) const { return compare(R) < 0; }
    utils::TraceClass& getTrace() const;

   private:
    const Case* C;
    decode::IntType Min;
    decode::IntType Max;
    uint32_t ShiftValue;
  };

  bool validateNode(ConstNodeVectorType& Paremts) const OVERRIDE;
  typedef std::vector<WriteRange> CaseRangeVectorType;
  mutable CaseRangeVectorType CaseRangeVector;
};

class BinaryEval : public Unary {
  BinaryEval() = delete;
  BinaryEval(const BinaryEval&) = delete;
  BinaryEval& operator=(const BinaryEval&) = delete;

 public:
  explicit BinaryEval(SymbolTable& Symtab, Node* Encoding);
  ~BinaryEval() OVERRIDE;

  const Node* getEncoding(decode::IntType Value) const;
  bool addEncoding(const BinaryAccept* Encoding) const;

  static bool implementsClass(NodeType Type) {
    return NodeType::BinaryEval == Type;
  }

 private:
  IntLookup* getIntLookup() const;
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_AST_H_
