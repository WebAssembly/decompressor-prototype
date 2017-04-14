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

#include "sexp/Ast.h"

#include <algorithm>

#include "interp/IntFormats.h"
#include "sexp/TextWriter.h"
#include "stream/WriteUtils.h"
#include "utils/Casting.h"
#include "utils/Trace.h"

#include "sexp/Ast-templates.h"

#define DEBUG_FILE 0

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace utils {

void TraceClass::trace_node_ptr(const char* Name, const Node* Nd) {
  indent();
  trace_value_label(Name);
  TextWriter Writer;
  Writer.writeAbbrev(File, Nd);
}

}  // end of namespace utils.

namespace filt {

namespace {

void errorDescribeContext(ConstNodeVectorType& Parents,
                          const char* Context = "Context",
                          bool Abbrev = true) {
  if (Parents.empty())
    return;
  TextWriter Writer;
  FILE* Out = Parents[0]->getErrorFile();
  fprintf(Out, "%s:\n", Context);
  for (size_t i = Parents.size() - 1; i > 0; --i)
    if (Abbrev)
      Writer.writeAbbrev(Out, Parents[i - 1]);
    else
      Writer.write(Out, Parents[i - 1]);
}

void errorDescribeNode(const char* Message,
                       const Node* Nd,
                       bool Abbrev = true) {
  TextWriter Writer;
  FILE* Out = Nd->getErrorFile();
  if (Message)
    fprintf(Out, "%s:\n", Message);
  if (Abbrev)
    Writer.writeAbbrev(Out, Nd);
  else
    Writer.write(Out, Nd);
}

void errorDescribeNodeContext(const char* Message,
                              const Node* Nd,
                              ConstNodeVectorType& Parents) {
  errorDescribeNode(Message, Nd);
  errorDescribeContext(Parents);
}

static const char* PredefinedName[NumPredefinedSymbols]{"Unknown"
#define X(tag, name) , name
                                                        PREDEFINED_SYMBOLS_TABLE
#undef X
};

bool extractIntTypeFormat(const Node* Nd, IntTypeFormat& Format) {
  Format = IntTypeFormat::Uint8;
  if (Nd == nullptr)
    return false;
  switch (Nd->getType()) {
    case kU8Const:
      Format = IntTypeFormat::Uint8;
      return true;
    case kI32Const:
      Format = IntTypeFormat::Varint32;
      return true;
    case kU32Const:
      Format = IntTypeFormat::Uint32;
      return true;
    case kI64Const:
      Format = IntTypeFormat::Varint64;
      return true;
    case kU64Const:
      Format = IntTypeFormat::Uint64;
      return true;
    default:
      return false;
  }
}

bool compareSymbolNodesLt(const Symbol* S1, const Symbol* S2) {
  return S1->getName() < S2->getName();
}

}  // end of anonymous namespace

PredefinedSymbol toPredefinedSymbol(uint32_t Value) {
  if (Value < NumPredefinedSymbols)
    return PredefinedSymbol(Value);
  return PredefinedSymbol::Unknown;
}

const char* getName(PredefinedSymbol Sym) {
  uint32_t Index = uint32_t(Sym);
  assert(Index < NumPredefinedSymbols);
  return PredefinedName[Index];
}

AstTraitsType AstTraits[NumNodeTypes] = {
#define X(tag, opcode, sexp_name, text_num_args, text_max_args, NSL, hidden) \
  { k##tag, #tag, sexp_name, text_num_args, text_max_args, NSL, hidden }     \
  ,
    AST_OPCODE_TABLE
#undef X
};

const AstTraitsType* getAstTraits(NodeType Type) {
  static std::unordered_map<int, AstTraitsType*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < NumNodeTypes; ++i) {
      AstTraitsType* Traits = &AstTraits[i];
      Mapping[int(Traits->Type)] = Traits;
    }
  }
  AstTraitsType* Traits = Mapping[Type];
  if (Traits)
    return Traits;
  // Unknown case, make up entry
  Traits = new AstTraitsType();
  Traits->Type = Type;
  std::string NewName(std::string("NodeType::") +
                      std::to_string(static_cast<int>(Type)));
  char* Name = new char[NewName.size() + 1];
  Name[NewName.size()] = '\0';
  memcpy(Name, NewName.data(), NewName.size());
  Traits->TypeName = Name;
  Traits->SexpName = Name;
  Traits->NumTextArgs = 1;
  Traits->AdditionalTextArgs = 0;
  Traits->NeverSameLineInText = false;
  Traits->HidesSeqInText = false;
  Mapping[Type] = Traits;
  return Traits;
}

const char* getNodeSexpName(NodeType Type) {
  const AstTraitsType* Traits = getAstTraits(Type);
  charstring Name = Traits->SexpName;
  if (Name)
    return Name;
  Name = Traits->TypeName;
  if (Name)
    return Name;
  return "?Unknown?";
}

const char* getNodeTypeName(NodeType Type) {
  const AstTraitsType* Traits = getAstTraits(Type);
  charstring Name = Traits->TypeName;
  if (Name)
    return Name;
  Name = Traits->SexpName;
  if (Name)
    return Name;
  return "?Unknown?";
}

Node::Iterator::Iterator(const Node* Nd, int Index) : Nd(Nd), Index(Index) {
}

Node::Iterator::Iterator(const Iterator& Iter)
    : Nd(Iter.Nd), Index(Iter.Index) {
}

Node::Iterator& Node::Iterator::operator=(const Iterator& Iter) {
  Nd = Iter.Nd;
  Index = Iter.Index;
  return *this;
}

bool Node::Iterator::operator==(const Iterator& Iter) {
  return Nd == Iter.Nd && Index == Iter.Index;
}

bool Node::Iterator::operator!=(const Iterator& Iter) {
  return Nd != Iter.Nd || Index != Iter.Index;
}

Node* Node::Iterator::operator*() const {
  return Nd->getKid(Index);
}

Node::Node(SymbolTable& Symtab, NodeType Type)
    : Type(Type), Symtab(Symtab), CreationIndex(Symtab.getNextCreationIndex()) {
}

Node::~Node() {
}

int Node::compare(const Node* Nd) const {
  if (this == Nd)
    return 0;
  int Diff = nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  // Structurally compare subtrees. Note; we assume that if nodeCompare()==0,
  // the node must have the same number of children.
  std::vector<const Node*> Frontier;
  Frontier.push_back(this);
  Frontier.push_back(Nd);
  while (!Frontier.empty()) {
    const Node* Nd2 = Frontier.back();
    Frontier.pop_back();
    assert(!Frontier.empty());
    const Node* Nd1 = Frontier.back();
    Frontier.pop_back();
    assert(Nd1->getNumKids() == Nd2->getNumKids());
    for (int i = 0, Size = Nd1->getNumKids(); i < Size; ++i) {
      const Node* Kid1 = Nd1->getKid(i);
      const Node* Kid2 = Nd2->getKid(i);
      if (Kid1 == Kid2)
        continue;
      Diff = Kid1->nodeCompare(Kid2);
      if (Diff != 0)
        return Diff;
      Frontier.push_back(Kid1);
      Frontier.push_back(Kid2);
    }
  }
  return 0;
}

int Node::nodeCompare(const Node* Nd) const {
  return int(Type) - int(Nd->Type);
}

int Node::compareIncomparable(const Node* Nd) const {
  // First use creation index to try and make value consistent between runs.
  int Diff = getCreationIndex() - Nd->getCreationIndex();
  if (Diff != 0)
    return Diff;
  // At this point, drop to implementation detail.
  if (this < Nd)
    return -1;
  if (this > Nd)
    return 1;
  return 0;
}

bool Node::definesIntTypeFormat() const {
  IntTypeFormat Format;
  return extractIntTypeFormat(this, Format);
}

const char* Node::getName() const {
  return getNodeSexpName(getType());
}

const char* Node::getNodeName() const {
  return getNodeTypeName(getType());
}

void Node::setLastKid(Node* N) {
  setKid(getNumKids() - 1, N);
}

Node* Node::getLastKid() const {
  if (int Size = getNumKids())
    return getKid(Size - 1);
  return nullptr;
}

Node::Iterator Node::begin() const {
  return Iterator(this, 0);
}

Node::Iterator Node::end() const {
  return Iterator(this, getNumKids());
}

Node::Iterator Node::rbegin() const {
  return Iterator(this, getNumKids() - 1);
}

Node::Iterator Node::rend() const {
  return Iterator(this, -1);
}

IntTypeFormat Node::getIntTypeFormat() const {
  IntTypeFormat Format;
  extractIntTypeFormat(this, Format);
  return Format;
}

size_t Node::getTreeSize() const {
  size_t Count = 0;
  std::vector<const Node*> ToVisit;
  ToVisit.push_back(this);
  while (!ToVisit.empty()) {
    const Node* Nd = ToVisit.back();
    ToVisit.pop_back();
    ++Count;
    for (const Node* Kid : *Nd)
      ToVisit.push_back(Kid);
  }
  return Count;
}

IntegerValue::IntegerValue()
    : Type(NO_SUCH_NODETYPE),
      Value(0),
      Format(decode::ValueFormat::Decimal),
      isDefault(false) {
}

IntegerValue::IntegerValue(decode::IntType Value, decode::ValueFormat Format)
    : Type(NO_SUCH_NODETYPE), Value(Value), Format(Format), isDefault(false) {
}

IntegerValue::IntegerValue(NodeType Type,
                           decode::IntType Value,
                           decode::ValueFormat Format,
                           bool isDefault)
    : Type(Type), Value(Value), Format(Format), isDefault(isDefault) {
}

IntegerValue::IntegerValue(const IntegerValue& V)
    : Type(V.Type), Value(V.Value), Format(V.Format), isDefault(V.isDefault) {
}

void IntegerValue::describe(FILE* Out) const {
  fprintf(Out, "%s<", getNodeSexpName(Type));
  writeInt(Out, Value, Format);
  fprintf(Out, ", %s>", getName(Format));
}

IntegerValue::~IntegerValue() {
}

int IntegerValue::compare(const IntegerValue& V) const {
  if (Type < V.Type)
    return -1;
  if (Type > V.Type)
    return 1;
  if (Value < V.Value)
    return -1;
  if (Value > V.Value)
    return 1;
  if (Format < V.Format)
    return -1;
  if (Format > V.Format)
    return 1;
  return int(isDefault) - int(V.isDefault);
}

void Node::append(Node*) {
  decode::fatal("Node::append not supported for ast node!");
}

bool Node::validateKid(ConstNodeVectorType& Parents, const Node* Kid) const {
  Parents.push_back(this);
  bool Result = Kid->validateSubtree(Parents);
  Parents.pop_back();
  return Result;
}

bool Node::validateKids(ConstNodeVectorType& Parents) const {
  if (!hasKids())
    return true;
  TRACE(int, "NumKids", getNumKids());
  for (auto* Kid : *this)
    if (!validateKid(Parents, Kid))
      return false;
  return true;
}

bool Node::validateSubtree(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateSubtree");
  TRACE(node_ptr, nullptr, this);
  if (!validateNode(Parents))
    return false;
  return validateKids(Parents);
}

bool Node::validateNode(ConstNodeVectorType& Scope) const {
  TRACE_METHOD("validateNode");
  return true;
}

Cached::Cached(SymbolTable& Symtab, NodeType Type) : Nullary(Symtab, Type) {
}

Cached::~Cached() {
}

int Cached::nodeCompare(const Node* Nd) const {
  int Diff = Nullary::nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  return compareIncomparable(Nd);
}

bool Cached::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag) case k##tag:
      AST_CACHEDNODE_TABLE
#undef X
      return true;
  }
}

IntLookup::IntLookup(SymbolTable& Symtab) : Cached(Symtab, kIntLookup) {
}

IntLookup::~IntLookup() {
}

const Node* IntLookup::get(decode::IntType Value) {
  if (Lookup.count(Value))
    return Lookup[Value];
  return nullptr;
}

bool IntLookup::add(decode::IntType Value, const Node* Nd) {
  if (Lookup.count(Value))
    return false;
  Lookup[Value] = Nd;
  return true;
}

SymbolDefn::SymbolDefn(SymbolTable& Symtab)
    : Cached(Symtab, kSymbolDefn),
      Symbol(nullptr),
      DefineDefinition(nullptr),
      LiteralDefinition(nullptr),
      LiteralActionDefinition(nullptr) {
}

SymbolDefn::~SymbolDefn() {
}

const std::string& SymbolDefn::getName() const {
  if (Symbol)
    return Symbol->getName();
  static std::string Unknown("???");
  return Unknown;
}

const Define* SymbolDefn::getDefineDefinition() const {
  if (DefineDefinition)
    return DefineDefinition;
  // Not defined locally, find enclosing definition.
  if (Symbol == nullptr)
    return nullptr;
  const std::string& Name = Symbol->getName();
  for (SymbolTable* Scope = &Symtab; Scope != nullptr;
       Scope = Scope->getEnclosingScope().get()) {
    SymbolDefn* SymDef = Scope->getSymbolDefn(Scope->getOrCreateSymbol(Name));
    if (SymDef == nullptr)
      continue;
    if (SymDef->DefineDefinition)
      return DefineDefinition = SymDef->DefineDefinition;
  }
  return nullptr;
}

void SymbolDefn::setDefineDefinition(const Define* Defn) {
  if (DefineDefinition) {
    errorDescribeNode("Old", DefineDefinition);
    errorDescribeNode("New", Defn);
    fatal("Multiple defines for symbol: " + getName());
    return;
  }
  DefineDefinition = Defn;
}

void SymbolDefn::setLiteralDefinition(const LiteralDef* Defn) {
  if (LiteralDefinition) {
    errorDescribeNode("Old", LiteralDefinition);
    errorDescribeNode("New", Defn);
    fatal("Multiple defines for symbol: " + getName());
    return;
  }
  LiteralDefinition = Defn;
}

const LiteralDef* SymbolDefn::getLiteralDefinition() const {
  if (LiteralDefinition)
    return LiteralDefinition;
  // Not defined locally, find enclosing definition.
  if (Symbol == nullptr)
    return nullptr;
  const std::string& Name = Symbol->getName();
  for (SymbolTable* Scope = &Symtab; Scope != nullptr;
       Scope = Scope->getEnclosingScope().get()) {
    SymbolDefn* SymDef = Scope->getSymbolDefn(Scope->getOrCreateSymbol(Name));
    if (SymDef == nullptr)
      continue;
    if (SymDef->LiteralDefinition)
      return LiteralDefinition = SymDef->LiteralDefinition;
  }
  return nullptr;
}

void SymbolDefn::setLiteralActionDefinition(const LiteralActionDef* Defn) {
  if (LiteralActionDefinition) {
    errorDescribeNode("Old", LiteralActionDefinition);
    errorDescribeNode("New", Defn);
    fatal("Multiple action defines for symbol: " + getName());
    return;
  }
  LiteralActionDefinition = Defn;
}

const LiteralActionDef* SymbolDefn::getLiteralActionDefinition() const {
  if (LiteralActionDefinition)
    return LiteralActionDefinition;
  // Not defined locally, find enclosing definition.
  if (Symbol == nullptr)
    return nullptr;
  const std::string& Name = Symbol->getName();
  for (SymbolTable* Scope = &Symtab; Scope != nullptr;
       Scope = Scope->getEnclosingScope().get()) {
    SymbolDefn* SymDef = Scope->getSymbolDefn(Scope->getOrCreateSymbol(Name));
    if (SymDef == nullptr)
      continue;
    if (SymDef->LiteralActionDefinition)
      return LiteralActionDefinition = SymDef->LiteralActionDefinition;
  }
  return nullptr;
}

Symbol::Symbol(SymbolTable& Symtab, const std::string& Name)
    : Nullary(Symtab, kSymbol), Name(Name) {
  init();
}

Symbol::~Symbol() {
}

void Symbol::init() {
  PredefinedValue = PredefinedSymbol::Unknown;
  PredefinedValueIsCached = false;
}

int Symbol::nodeCompare(const Node* Nd) const {
  int Diff = Nullary::nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<Symbol>(Nd));
  const auto* SymNd = cast<Symbol>(Nd);
  return Name.compare(SymNd->Name);
}

SymbolDefn* Symbol::getSymbolDefn() const {
  SymbolDefn* Defn = cast<SymbolDefn>(Symtab.getCachedValue(this));
  if (Defn == nullptr) {
    Defn = Symtab.create<SymbolDefn>();
    Defn->setSymbol(this);
    Symtab.setCachedValue(this, Defn);
  }
  return Defn;
}

void Symbol::setPredefinedSymbol(PredefinedSymbol NewValue) {
  if (PredefinedValueIsCached)
    fatal(std::string("Can't define \"") + filt::getName(PredefinedValue) +
          " and " + filt::getName(NewValue));
  PredefinedValue = NewValue;
  PredefinedValueIsCached = true;
}

SymbolTable::SymbolTable(std::shared_ptr<SymbolTable> EnclosingScope)
    : EnclosingScope(EnclosingScope) {
  init();
}

SymbolTable::SymbolTable() {
  init();
}

void SymbolTable::init() {
  setRoot(nullptr);
  NextCreationIndex = 0;
  ActionBase = 0;
  Err = create<Error>();
  BlockEnterCallback = nullptr;
  BlockExitCallback = nullptr;
}

SymbolTable::~SymbolTable() {
  clearSymbols();
  deallocateNodes();
}

const Callback* SymbolTable::getBlockEnterCallback() {
  if (BlockEnterCallback == nullptr)
    BlockEnterCallback =
        create<Callback>(getPredefined(PredefinedSymbol::Block_enter));
  return BlockEnterCallback;
}

const Callback* SymbolTable::getBlockExitCallback() {
  if (BlockExitCallback == nullptr)
    BlockExitCallback =
        create<Callback>(getPredefined(PredefinedSymbol::Block_exit));
  return BlockExitCallback;
}

FILE* SymbolTable::getErrorFile() const {
  return stderr;
}

FILE* SymbolTable::error() const {
  FILE* Out = getErrorFile();
  fputs("Error: ", Out);
  return Out;
}

Symbol* SymbolTable::getSymbol(const std::string& Name) {
  if (SymbolMap.count(Name))
    return SymbolMap[Name];
  return nullptr;
}

SymbolDefn* SymbolTable::getSymbolDefn(const Symbol* Sym) {
  SymbolDefn* Defn = cast<SymbolDefn>(getCachedValue(Sym));
  if (Defn == nullptr) {
    Defn = create<SymbolDefn>();
    Defn->setSymbol(Sym);
    setCachedValue(Sym, Defn);
  }
  return Defn;
}

void SymbolTable::insertCallbackLiteral(const LiteralActionDef* Defn) {
  CallbackLiterals.insert(Defn);
}

void SymbolTable::insertCallbackValue(const IntegerNode* IntNd) {
  CallbackValues.insert(IntNd);
}

void SymbolTable::collectActionDefs(ActionDefSet& DefSet) {
  SymbolTable* Scope = this;
  while (Scope) {
    for (const LiteralActionDef* Def : Scope->CallbackLiterals)
      DefSet.insert(Def);
    Scope = Scope->getEnclosingScope().get();
  }
}

void SymbolTable::clearSymbols() {
  SymbolMap.clear();
}

void SymbolTable::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  getTrace().setTraceProgress(NewValue);
}

void SymbolTable::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClass> SymbolTable::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("SymbolTable"));
  return Trace;
}

void SymbolTable::deallocateNodes() {
  for (Node* Nd : Allocated)
    delete Nd;
}

Symbol* SymbolTable::getOrCreateSymbol(const std::string& Name) {
  Symbol* Nd = SymbolMap[Name];
  if (Nd == nullptr) {
    Nd = new Symbol(*this, Name);
    Allocated.push_back(Nd);
    SymbolMap[Name] = Nd;
  }
  return Nd;
}

Symbol* SymbolTable::getPredefined(PredefinedSymbol Sym) {
  Symbol* Nd = PredefinedMap[Sym];
  if (Nd != nullptr)
    return Nd;
  Nd = getOrCreateSymbol(PredefinedName[uint32_t(Sym)]);
  Nd->setPredefinedSymbol(Sym);
  PredefinedMap[Sym] = Nd;
  return Nd;
}

#define X(tag, format, defval, mergable, BASE, NODE_DECLS)           \
  template <>                                                        \
  tag* SymbolTable::create<tag>(IntType Value, ValueFormat Format) { \
    if (mergable) {                                                  \
      IntegerValue I(k##tag, Value, Format, false);                  \
      BASE* Nd = IntMap[I];                                          \
      if (Nd == nullptr) {                                           \
        Nd = new tag(*this, Value, Format);                          \
        Allocated.push_back(Nd);                                     \
        IntMap[I] = Nd;                                              \
      }                                                              \
      return dyn_cast<tag>(Nd);                                      \
    }                                                                \
    tag* Nd = new tag(*this, Value, Format);                         \
    Allocated.push_back(Nd);                                         \
    return Nd;                                                       \
  }                                                                  \
  template <>                                                        \
  tag* SymbolTable::create<tag>() {                                  \
    if (mergable) {                                                  \
      IntegerValue I(k##tag, (defval), ValueFormat::Decimal, true);  \
      BASE* Nd = IntMap[I];                                          \
      if (Nd == nullptr) {                                           \
        Nd = new tag(*this);                                         \
        Allocated.push_back(Nd);                                     \
        IntMap[I] = Nd;                                              \
      }                                                              \
      return dyn_cast<tag>(Nd);                                      \
    }                                                                \
    tag* Nd = new tag(*this);                                        \
    Allocated.push_back(Nd);                                         \
    return Nd;                                                       \
  }
AST_INTEGERNODE_TABLE
#undef X

bool SymbolTable::areActionsConsistent() {
#if DEBUG_FILE
  // Debugging information.
  fprintf(stderr, "******************\n");
  fprintf(stderr, "Symbolic actions:\n");
  TextWriter Writer;
  for (const LiteralActionDef* Def : CallbackLiterals) {
    Writer.write(stderr, Def);
  }
  fprintf(stderr, "Hard coded actions:\n");
  for (const IntegerNode* Val : CallbackValues) {
    Writer.write(stderr, Val);
  }
  fprintf(stderr, "Undefined actions:\n");
  for (const Symbol* Sym : UndefinedCallbacks) {
    Writer.write(stderr, Sym);
  }
  fprintf(stderr, "******************\n");
#endif
  std::map<IntType, const Node*> DefMap;
  // First install hard coded (ignoring duplicates).
  for (const IntegerNode* IntNd : CallbackValues) {
    DefMap[IntNd->getValue()] = IntNd;
  }
  // Create values for undefined actions.
  bool IsValid = true;              // Until proven otherwise.
  constexpr IntType EnumGap = 100;  // gap for future expansion
  IntType NextEnumValue =
      ActionBase ? ActionBase : NumPredefinedSymbols + EnumGap;
  for (const LiteralActionDef* Def : CallbackLiterals) {
    const IntegerNode* IntNd = dyn_cast<IntegerNode>(Def->getKid(1));
    if (IntNd == nullptr) {
      errorDescribeNode("Unable to extract action value", Def);
      IsValid = false;
    }
    IntType Value = IntNd->getValue();
    if (Value >= NextEnumValue)
      NextEnumValue = Value + 1;
  }
  std::vector<const Symbol*> SortedSyms(UndefinedCallbacks.begin(),
                                        UndefinedCallbacks.end());
  std::sort(SortedSyms.begin(), SortedSyms.end(), compareSymbolNodesLt);
  for (const Symbol* Sym : SortedSyms) {
    SymbolDefn* SymDef = getSymbolDefn(Sym);
    const LiteralActionDef* LitDef = SymDef->getLiteralActionDefinition();
    if (LitDef != nullptr) {
      errorDescribeNode("Malformed undefined action", LitDef);
      IsValid = false;
      continue;
    }
    Node* SymNd = const_cast<Symbol*>(Sym);
    auto* Def = create<LiteralActionDef>(
        SymNd, create<U64Const>(NextEnumValue++, ValueFormat::Decimal));
    installDefinitions(Def);
    CallbackLiterals.insert(Def);
  }
  // Now see if conflicting definitions.
  for (const LiteralActionDef* Def : CallbackLiterals) {
    const auto* IntNd = dyn_cast<IntegerNode>(Def->getKid(1));
    if (IntNd == nullptr) {
      errorDescribeNode("Unable to extract action value", Def);
      IsValid = false;
      continue;
    }
    IntType Value = IntNd->getValue();
    if (DefMap.count(Value) == 0) {
      DefMap[Value] = Def;
      continue;
    }
    const Node* IntDef = DefMap[Value];
    // Before complaining about conflicting action values, ignore predefined
    // symbols. We do this because we always define them so that the predefined
    // actions will always work.
    if (const auto* Sym = dyn_cast<Symbol>(Def->getKid(0))) {
      if (Sym->isPredefinedSymbol())
        continue;
    }
    FILE* Out = IntNd->getErrorFile();
    fprintf(Out, "Conflicting action values:\n");
    TextWriter Writer;
    Writer.write(Out, IntDef);
    fprintf(Out, "and\n");
    Writer.write(Out, Def);
    IsValid = false;
  }
  return IsValid;
}

void SymbolTable::setRoot(const File* NewRoot) {
  Root = const_cast<File*>(NewRoot);
  RootInstalled = false;
}

bool SymbolTable::install() {
  TRACE_METHOD("install");
  if (RootInstalled)
    return true;
  CachedValue.clear();
  UndefinedCallbacks.clear();
  CallbackValues.clear();
  CallbackLiterals.clear();
  ActionBase = 0;
  if (Root == nullptr)
    return false;
  if (EnclosingScope && !EnclosingScope->isRootInstalled()) {
    if (!EnclosingScope->install())
      return false;
  }
  installPredefined();
  installDefinitions(Root);
  ConstNodeVectorType Parents;
  bool IsValid = Root->validateSubtree(Parents);
  if (IsValid)
    IsValid = areActionsConsistent();
  if (!IsValid)
    fatal("Unable to install algorthms, validation failed!");
  return RootInstalled = true;
}

const Header* SymbolTable::getSourceHeader() const {
  if (Root == nullptr)
    return nullptr;
  return Root->getSourceHeader();
}

const Header* SymbolTable::getReadHeader() const {
  if (Root == nullptr)
    return nullptr;
  return Root->getReadHeader();
}

const Header* SymbolTable::getWriteHeader() const {
  if (Root == nullptr)
    return nullptr;
  return Root->getWriteHeader();
}

bool SymbolTable::specifiesAlgorithm() const {
  if (Root == nullptr)
    return false;
  return *Root->getSourceHeader() == *Root->getReadHeader() &&
         *Root->getReadHeader() == *Root->getWriteHeader();
}

void SymbolTable::installPredefined() {
  for (uint32_t i = 0; i < NumPredefinedSymbols; ++i) {
    Symbol* Sym = getPredefined(toPredefinedSymbol(i));
    U32Const* Const = create<U32Const>(i, ValueFormat::Decimal);
    const auto* Def = create<LiteralActionDef>(Sym, Const);
    Sym->setLiteralActionDefinition(Def);
    insertCallbackLiteral(Def);
  }
}

void SymbolTable::installDefinitions(const Node* Root) {
  TRACE_METHOD("installDefinitions");
  TRACE(node_ptr, nullptr, Root);
  if (Root == nullptr)
    return;
  switch (Root->getType()) {
    default:
      return;
    case kFile:
    case kSection:
      for (Node* Kid : *Root)
        installDefinitions(Kid);
      return;
    case kDefine: {
      if (auto* DefineSymbol = dyn_cast<Symbol>(Root->getKid(0)))
        return DefineSymbol->setDefineDefinition(cast<Define>(Root));
      errorDescribeNode("Malformed define", Root);
      fatal("Malformed define s-expression found!");
      return;
    }
    case kLiteralDef: {
      if (auto* LiteralSymbol = dyn_cast<Symbol>(Root->getKid(0)))
        return LiteralSymbol->setLiteralDefinition(cast<LiteralDef>(Root));
      errorDescribeNode("Malformed", Root);
      fatal("Malformed literal s-expression found!");
      return;
    }
    case kLiteralActionBase: {
      const auto* IntNd = cast<IntegerNode>(Root->getKid(0));
      if (IntNd == nullptr)
        errorDescribeNode("Unable to extract literal action base", Root);
      IntType Base = IntNd->getValue();
      if (ActionBase != 0) {
        fprintf(Root->getErrorFile(), "Literal action base was: %" PRIuMAX "\n",
                uintmax_t(ActionBase));
        errorDescribeNode("Redefining to", Root);
        fatal("Duplicate literal action bases defined!");
      }
      ActionBase = Base;
      for (int i = 1, NumKids = Root->getNumKids(); i < NumKids; ++i) {
        auto* Sym = dyn_cast<Symbol>(Root->getKid(i));
        if (Sym == nullptr) {
          errorDescribeNode("Symbol expected", Root->getKid(1));
          errorDescribeNode("In", Root);
          return fatal("Unable to install algorithm");
        }
        Node* Value = create<U64Const>(Base, IntNd->getFormat());
        Node* Lit = create<LiteralActionDef>(Sym, Value);
        installDefinitions(Lit);
        ++Base;
      }
      return;
    }
    case kLiteralActionDef: {
      if (auto* LiteralSymbol = dyn_cast<Symbol>(Root->getKid(0))) {
        if (LiteralSymbol->isPredefinedSymbol()) {
          errorDescribeNode("In", Root);
          return fatal("Can't redefine predefined symbol");
        }
        const auto* Def = cast<LiteralActionDef>(Root);
        insertCallbackLiteral(Def);
        return LiteralSymbol->setLiteralActionDefinition(Def);
      }
      errorDescribeNode("Malformed", Root);
      return fatal("Malformed literal s-expression found!");
    }
    case kRename: {
      if (auto* OldSymbol = dyn_cast<Symbol>(Root->getKid(0))) {
        if (auto* NewSymbol = dyn_cast<Symbol>(Root->getKid(1))) {
          const Define* Defn = OldSymbol->getDefineDefinition();
          NewSymbol->setDefineDefinition(Defn);
          return;
        }
      }
      errorDescribeNode("Malformed", Root);
      fatal("Malformed rename s-expression found!");
      return;
    }
    case kUndefine: {
      if (auto* UndefineSymbol = dyn_cast<Symbol>(Root->getKid(0))) {
        UndefineSymbol->setDefineDefinition(nullptr);
        return;
      }
      errorDescribeNode("Can't undefine", Root);
      fatal("Malformed undefine s-expression found!");
      return;
    }
  }
}

TraceClass& SymbolTable::getTrace() {
  return *getTracePtr();
}

void SymbolTable::describe(FILE* Out, bool ShowInternalStructure) {
  TextWriter Writer;
  Writer.setShowInternalStructure(ShowInternalStructure);
  Writer.write(Out, this);
}

void SymbolTable::stripCallbacksExcept(std::set<std::string>& KeepActions) {
  setRoot(dyn_cast<File>(stripCallbacksExcept(KeepActions, Root)));
}

void SymbolTable::stripSymbolicCallbacks() {
  setRoot(dyn_cast<File>(stripSymbolicCallbackUses(Root)));
  if (Root != nullptr)
    setRoot(dyn_cast<File>(stripSymbolicCallbackDefs(Root)));
}

void SymbolTable::stripLiterals() {
  stripLiteralUses();
  stripLiteralDefs();
}

void SymbolTable::stripLiteralUses() {
  setRoot(dyn_cast<File>(stripLiteralUses(Root)));
}

void SymbolTable::stripLiteralDefs() {
  SymbolSet DefSyms;
  collectLiteralUseSymbols(DefSyms);
  setRoot(dyn_cast<File>(stripLiteralDefs(Root, DefSyms)));
}

Node* SymbolTable::stripUsing(Node* Root,
                              std::function<Node*(Node*)> stripKid) {
  switch (NodeType Op = Root->getType()) {
    default:
      for (int i = 0; i < Root->getNumKids(); ++i)
        Root->setKid(i, stripKid(Root->getKid(i)));
      return Root;
#define X(tag, BASE, NODE_DECLS) case k##tag:
      AST_NARYNODE_TABLE
#undef X
      {
        // TODO: Make strip functions return nullptr to remove!
        std::vector<Node*> Kids;
        int index = 0;
        int limit = Root->getNumKids();
        if (Op == kFile) {
          // Note: Never remove void's in a file node (They represent
          // header information). Only process once declarations (i.e.
          // a section node) are reached.
          for (; index < limit; ++index) {
            Node* Kid = Root->getKid(index);
            if (isa<Section>(Kid))
              break;
            Kids.push_back(Kid);
          }
        }
        // Simplify kids, removing "void" operations from the nary node.
        for (; index < limit; ++index) {
          Node* Kid = stripKid(Root->getKid(index));
          if (!isa<Void>(Kid))
            Kids.push_back(Kid);
        }
        if (Kids.size() == size_t(Root->getNumKids())) {
          // Replace kids in place.
          for (size_t i = 0; i < Kids.size(); ++i)
            Root->setKid(i, Kids[i]);
          return Root;
        }
        if (Kids.empty())
          break;
        if (Kids.size() == 1 && Root->getType() == kSequence)
          return Kids[0];
        Nary* Nd = dyn_cast<Nary>(Root);
        if (Nd == nullptr)
          break;
        Nd->clearKids();
        for (auto Kid : Kids)
          Nd->append(Kid);
        return Nd;
      }
  }
  return create<Void>();
}

Node* SymbolTable::stripCallbacksExcept(std::set<std::string>& KeepActions,
                                        Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(Root, [&](Node* Nd) -> Node* {
        return stripCallbacksExcept(KeepActions, Nd);
      });
    case kCallback: {
      Node* Action = Root->getKid(0);
      switch (Action->getType()) {
        default:
          return Root;
        case kLiteralActionUse: {
          auto* Sym = dyn_cast<Symbol>(Action->getKid(0));
          if (Sym == nullptr)
            return Root;
          if (Sym->isPredefinedSymbol() || KeepActions.count(Sym->getName()))
            return Root;
          break;
        }
      }
      break;
    }
    case kLiteralActionDef:
      if (const auto* Sym = dyn_cast<Symbol>(Root->getKid(0))) {
        if (KeepActions.count(Sym->getName()))
          return Root;
      }
      break;
    case kLiteralActionBase: {
      bool CanRemove = true;
      for (int i = 1; i < Root->getNumKids(); ++i) {
        if (const auto* Sym = dyn_cast<Symbol>(Root->getKid(i))) {
          if (KeepActions.count(Sym->getName())) {
            CanRemove = false;
            break;
          }
        }
      }
      if (!CanRemove)
        return Root;
      break;
    }
  }
  return create<Void>();
}

Node* SymbolTable::stripSymbolicCallbackUses(Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(Root, [&](Node* Nd) -> Node* {
        return stripSymbolicCallbackUses(Nd);
      });
    case kLiteralActionUse: {
      const auto* Sym = dyn_cast<Symbol>(Root->getKid(0));
      if (Sym == nullptr)
        return Root;
      const auto* Def = Sym->getLiteralActionDefinition();
      if (Def == nullptr)
        break;
      return Def->getKid(1);
    }
  }
  // If reached, this is a symbolic action use without a def, so remove.
  TextWriter Writer;
  FILE* Out = error();
  fprintf(Out, "No action definition for: ");
  Writer.write(Out, Root);
  return create<Void>();
}

Node* SymbolTable::stripSymbolicCallbackDefs(Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(Root, [&](Node* Nd) -> Node* {
        return stripSymbolicCallbackDefs(Nd);
      });
    case kLiteralActionDef:
      break;
    case kLiteralActionBase:
      break;
  }
  return create<Void>();
}

Node* SymbolTable::stripLiteralUses(Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(
          Root, [&](Node* Nd) -> Node* { return stripLiteralUses(Nd); });
    case kLiteralActionUse:
      return Root;
    case kLiteralUse: {
      const auto* Use = cast<LiteralUse>(Root);
      const auto* Sym = dyn_cast<Symbol>(Use->getKid(0));
      if (Sym == nullptr)
        break;
      const auto* Def = Sym->getLiteralDefinition();
      if (Def == nullptr)
        break;
      return Def->getKid(1);
    }
  }
  // If reached, this is a use without a def, so remove.
  TextWriter Writer;
  FILE* Out = error();
  fprintf(Out, "No literal definition for: ");
  Writer.write(Out, Root);
  return create<Void>();
}

void SymbolTable::collectLiteralUseSymbols(SymbolSet& Symbols) {
  ConstNodeVectorType ToVisit;
  ToVisit.push_back(Root);
  while (!ToVisit.empty()) {
    const Node* Nd = ToVisit.back();
    ToVisit.pop_back();
    for (const Node* Kid : *Nd)
      ToVisit.push_back(Kid);
    const auto* Use = dyn_cast<LiteralUse>(Nd);
    if (Use == nullptr)
      continue;
    const Node* Sym = Use->getKid(0);
    assert(isa<Symbol>(Sym));
    Symbols.insert(cast<Symbol>(Sym));
  }
}

Node* SymbolTable::stripLiteralDefs(Node* Root, SymbolSet& DefSyms) {
  switch (Root->getType()) {
    default:
      return stripUsing(Root, [&](Node* Nd) -> Node* {
        return stripLiteralDefs(Nd, DefSyms);
      });
    case kLiteralDef:
      if (DefSyms.count(dyn_cast<Symbol>(Root->getKid(0))))
        return Root;
      break;
    case kLiteralActionDef:
      if (CallbackLiterals.count(cast<LiteralActionDef>(Root)))
        return Root;
      break;
  }
  return create<Void>();
}

Nullary::Nullary(SymbolTable& Symtab, NodeType Type) : Node(Symtab, Type) {
}

Nullary::~Nullary() {
}

int Nullary::getNumKids() const {
  return 0;
}

Node* Nullary::getKid(int) const {
  return nullptr;
}

void Nullary::setKid(int, Node*) {
  decode::fatal("Nullary::setKid not allowed");
}

bool Nullary::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, BASE, NODE_DECLS) \
  case k##tag:                   \
    return true;
      AST_NULLARYNODE_TABLE
#undef X
  }
}

#define X(tag, BASE, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab) : BASE(Symtab, k##tag) {}
AST_NULLARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) template tag* SymbolTable::create<tag>();
AST_NULLARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_NULLARYNODE_TABLE
#undef X

Unary::Unary(SymbolTable& Symtab, NodeType Type, Node* Kid)
    : Node(Symtab, Type) {
  Kids[0] = Kid;
}

Unary::~Unary() {
}

int Unary::getNumKids() const {
  return 1;
}

Node* Unary::getKid(int Index) const {
  if (Index < 1)
    return Kids[0];
  return nullptr;
}

void Unary::setKid(int Index, Node* NewValue) {
  assert(Index < 1);
  Kids[0] = NewValue;
}

bool Unary::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
    case kBinaryEval:
#define X(tag, BASE, NODE_DECLS) \
  case k##tag:                   \
    return true;
      AST_UNARYNODE_TABLE
#undef X
  }
}

#define X(tag, BASE, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab, Node* Kid) : BASE(Symtab, k##tag, Kid) {}
AST_UNARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  template tag* SymbolTable::create<tag>(Node * Nd);
AST_UNARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_UNARYNODE_TABLE
#undef X

const LiteralDef* LiteralUse::getDef() const {
  return cast<Symbol>(getKid(0))->getLiteralDefinition();
}

const LiteralActionDef* LiteralActionUse::getDef() const {
  return cast<Symbol>(getKid(0))->getLiteralActionDefinition();
}

bool LiteralUse::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  if (getDef())
    return true;
  fprintf(getErrorFile(), "No corresponding literal definition found\n");
  return false;
}

bool LiteralActionUse::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  if (const LiteralActionDef* Def = getDef()) {
    getSymtab().insertCallbackLiteral(Def);
    return true;
  }
  const Node* SymNd = getKid(0);
  assert(isa<Symbol>(SymNd));
  getSymtab().insertUndefinedCallback(cast<Symbol>(SymNd));
  return true;
}

const IntegerNode* LiteralUse::getIntNode() const {
  const LiteralDef* Def = getDef();
  assert(Def != nullptr);
  const IntegerNode* IntNd = dyn_cast<IntegerNode>(Def->getKid(1));
  assert(IntNd != nullptr);
  return IntNd;
}

const IntegerNode* LiteralActionUse::getIntNode() const {
  const LiteralActionDef* Def = getDef();
  assert(Def != nullptr);
  const IntegerNode* IntNd = dyn_cast<IntegerNode>(Def->getKid(1));
  assert(IntNd != nullptr);
  return IntNd;
}

bool Callback::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  const Node* Action = getKid(0);
  if (const auto* IntNd = dyn_cast<IntegerNode>(Action)) {
    getSymtab().insertCallbackValue(IntNd);
    return true;
  }
  const auto* Use = dyn_cast<LiteralActionUse>(Action);
  if (Use == nullptr) {
    errorDescribeNodeContext("Malformed callback", this, Parents);
    return false;
  }
  return true;
}

const IntegerNode* Callback::getValue() const {
  const Node* Val = getKid(0);
  if (const auto* IntNd = dyn_cast<IntegerNode>(Val))
    return IntNd;
  if (const auto* Use = dyn_cast<LiteralActionUse>(Val))
    return Use->getIntNode();
  return nullptr;
}

IntegerNode::IntegerNode(SymbolTable& Symtab,
                         NodeType Type,
                         decode::IntType Value,
                         decode::ValueFormat Format,
                         bool isDefault)
    : Nullary(Symtab, Type), Value(Type, Value, Format, isDefault) {
}

IntegerNode::~IntegerNode() {
}

int IntegerNode::nodeCompare(const Node* Nd) const {
  int Diff = Nullary::nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<IntegerNode>(Nd));
  const auto* IntNd = cast<IntegerNode>(Nd);
  return Value.compare(IntNd->Value);
}

bool IntegerNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
    case kBinaryAccept:
#define X(tag, format, defval, mergable, BASE, NODE_DECLS) case k##tag:
      AST_INTEGERNODE_TABLE
#undef X
      return true;
  }
}

#define X(tag, format, defval, mergable, BASE, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab, decode::IntType Value,     \
           decode::ValueFormat Format)                     \
      : BASE(Symtab, k##tag, Value, Format, false) {}
AST_INTEGERNODE_TABLE
#undef X

#define X(tag, format, defval, mergable, BASE, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab)                            \
      : BASE(Symtab, k##tag, (defval), decode::ValueFormat::Decimal, true) {}
AST_INTEGERNODE_TABLE
#undef X

#define X(tag, format, defval, mergable, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_INTEGERNODE_TABLE
#undef X

bool Local::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  for (const Node* Nd : Parents) {
    if (const auto* Def = dyn_cast<Define>(Nd)) {
      TRACE(node_ptr, "Enclosing define", Def);
      if (Def->isValidLocal(getValue()))
        return true;
      errorDescribeNodeContext("Invalid local usage", this, Parents);
      return false;
    }
  }
  errorDescribeNodeContext("Not used within a define", this, Parents);
  return false;
}

bool Param::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  for (const Node* Nd : Parents) {
    if (const auto* Def = dyn_cast<Define>(Nd)) {
      TRACE(node_ptr, "Enclosing define", Def);
      if (Def->isValidParam(getValue()))
        return true;
      errorDescribeNodeContext("Invalid parameter usage", this, Parents);
      return false;
    }
    return true;
  }
  errorDescribeNodeContext("Not used within a define", this, Parents);
  return false;
}

BinaryAccept::BinaryAccept(SymbolTable& Symtab)
    : IntegerNode(Symtab,
                  kBinaryAccept,
                  0,
                  decode::ValueFormat::Hexidecimal,
                  true) {
}

BinaryAccept::BinaryAccept(SymbolTable& Symtab,
                           decode::IntType Value,
                           unsigned NumBits)
    : IntegerNode(Symtab,
                  kBinaryAccept,
                  Value,
                  decode::ValueFormat::Hexidecimal,
                  NumBits) {
}

BinaryAccept* SymbolTable::createBinaryAccept(IntType Value, unsigned NumBits) {
  BinaryAccept* Nd = new BinaryAccept(*this, Value, NumBits);
  Allocated.push_back(Nd);
  return Nd;
}

template BinaryAccept* SymbolTable::create<BinaryAccept>();

BinaryAccept::~BinaryAccept() {
}

int BinaryAccept::nodeCompare(const Node* Nd) const {
  int Diff = IntegerNode::nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<BinaryAccept>(Nd));
  const auto* BaNd = cast<BinaryAccept>(Nd);
  return int(NumBits) - int(BaNd->NumBits);
}

bool BinaryAccept::validateNode(ConstNodeVectorType& Parents) const {
  // Defines path (value) from leaf to (binary) root node, guaranteeing each
  // accept node has a unique value that can be case selected.
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  IntType MyValue = 0;
  unsigned MyNumBits = 0;
  const Node* LastNode = this;
  for (size_t i = Parents.size(); i > 0; --i) {
    const Node* Nd = Parents[i - 1];
    switch (Nd->getType()) {
      case kBinaryEval: {
        bool Success = true;
        if (!Value.isDefault &&
            (MyValue != Value.Value || MyNumBits != NumBits)) {
          FILE* Out = error();
          fprintf(Out, "Expected (%s ", getName());
          writeInt(Out, MyValue, ValueFormat::Hexidecimal);
          fprintf(Out, ":%u)\n", MyNumBits);
          errorDescribeNode("Malformed", this);
          Success = false;
        }
        TRACE(IntType, "Value", MyValue);
        TRACE(unsigned_int, "Bits", MyNumBits);
        Value.Value = MyValue;
        NumBits = MyNumBits;
        Value.isDefault = false;
        Value.Format = ValueFormat::Hexidecimal;
        if (!cast<BinaryEval>(Nd)->addEncoding(this)) {
          fprintf(error(), "Can't install opcode, malformed: %s\n", getName());
          Success = false;
        }
        return Success;
      }
      case kBinarySelect:
        if (MyNumBits >= sizeof(IntType) * CHAR_BIT) {
          FILE* Out = error();
          fprintf(Out, "Binary path too long for %s node\n", getName());
          return false;
        }
        MyValue <<= 1;
        if (LastNode == Nd->getKid(1))
          MyValue |= 1;
        LastNode = Nd;
        MyNumBits++;
        break;
      default: {
        // Exit loop and fail.
        FILE* Out = error();
        TextWriter Writer;
        Writer.write(Out, this);
        fprintf(Out, "Doesn't appear under %s\n",
                getNodeSexpName(NodeType::kBinaryEval));
        fprintf(Out, "Appears in:\n");
        Writer.write(Out, Nd);
        return false;
      }
    }
  }
  fprintf(error(), "%s can't appear at top level\n", getName());
  return false;
}

Binary::Binary(SymbolTable& Symtab, NodeType Type, Node* Kid1, Node* Kid2)
    : Node(Symtab, Type) {
  Kids[0] = Kid1;
  Kids[1] = Kid2;
}

Binary::~Binary() {
}

int Binary::getNumKids() const {
  return 2;
}

Node* Binary::getKid(int Index) const {
  if (Index < 2)
    return Kids[Index];
  return nullptr;
}

void Binary::setKid(int Index, Node* NewValue) {
  assert(Index < 2);
  Kids[Index] = NewValue;
}

bool Binary::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, BASE, NODE_DECLS) \
  case k##tag:                   \
    return true;
      AST_BINARYNODE_TABLE
#undef X
  }
}

#define X(tag, BASE, NODE_DECLS)                        \
  tag::tag(SymbolTable& Symtab, Node* Kid1, Node* Kid2) \
      : BASE(Symtab, k##tag, Kid1, Kid2) {}
AST_BINARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  template tag* SymbolTable::create<tag>(Node * Nd1, Node * Nd2);
AST_BINARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_BINARYNODE_TABLE
#undef X

Ternary::Ternary(SymbolTable& Symtab,
                 NodeType Type,
                 Node* Kid1,
                 Node* Kid2,
                 Node* Kid3)
    : Node(Symtab, Type) {
  Kids[0] = Kid1;
  Kids[1] = Kid2;
  Kids[2] = Kid3;
}

Ternary::~Ternary() {
}

int Ternary::getNumKids() const {
  return 3;
}

Node* Ternary::getKid(int Index) const {
  if (Index < 3)
    return Kids[Index];
  return nullptr;
}

void Ternary::setKid(int Index, Node* NewValue) {
  assert(Index < 3);
  Kids[Index] = NewValue;
}

bool Ternary::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, BASE, NODE_DECLS) \
  case k##tag:                   \
    return true;
      AST_TERNARYNODE_TABLE
#undef X
  }
}

#define X(tag, BASE, NODE_DECLS)                                    \
  tag::tag(SymbolTable& Symtab, Node* Kid1, Node* Kid2, Node* Kid3) \
      : BASE(Symtab, k##tag, Kid1, Kid2, Kid3) {}
AST_TERNARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  template tag* SymbolTable::create<tag>(Node * Nd1, Node * Nd2, Node * Nd3);
AST_TERNARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_TERNARYNODE_TABLE
#undef X

// Returns nullptr if P is illegal, based on the define.
bool Define::isValidParam(IntType Index) const {
  if (getNumKids() < 2)
    return false;
  if (!isa<Params>(getKid(1)))
    return false;
  return Index < cast<Params>(getKid(1))->getValue();
}

bool Define::isValidLocal(IntType Index) const {
  if (getNumKids() < 3)
    return false;
  if (!isa<Locals>(getKid(2)))
    return false;
  return Index < cast<Locals>(getKid(2))->getValue();
}

const std::string Define::getName() const {
  assert(getNumKids() == 0);
  assert(isa<Symbol>(getKid(0)));
  return cast<Symbol>(getKid(0))->getName();
}

size_t Define::getNumLocals() const {
  if (getNumKids() < 3)
    return false;
  if (auto* Locs = dyn_cast<Locals>(getKid(2)))
    return Locs->getValue();
  return 0;
}

Node* Define::getBody() const {
  assert(getNumKids() >= 3);
  Node* Nd = getKid(2);
  if (isa<Locals>(Nd)) {
    assert(getNumKids() >= 4);
    return getKid(3);
  }
  return Nd;
}

Nary::Nary(SymbolTable& Symtab, NodeType Type) : Node(Symtab, Type) {
}

Nary::~Nary() {
}

int Nary::nodeCompare(const Node* Nd) const {
  int Diff = Node::nodeCompare(Nd);
  if (Diff != 0)
    return Diff;
  return getNumKids() - Nd->getNumKids();
}

int Nary::getNumKids() const {
  return Kids.size();
}

Node* Nary::getKid(int Index) const {
  return Kids[Index];
}

void Nary::setKid(int Index, Node* N) {
  Kids[Index] = N;
}

void Nary::clearKids() {
  Kids.clear();
}

void Nary::append(Node* Kid) {
  Kids.emplace_back(Kid);
}

bool Nary::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, BASE, NODE_DECLS) \
  case k##tag:                   \
    return true;
      AST_NARYNODE_TABLE
#undef X
  }
}

#define X(tag, BASE, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab) : BASE(Symtab, k##tag) {}
AST_NARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) template tag* SymbolTable::create<tag>();
AST_NARYNODE_TABLE
#undef X

#define X(tag, BASE, NODE_DECLS) \
  tag::~tag() {}
AST_NARYNODE_TABLE
#undef X

Header::Header(SymbolTable& Symtab, NodeType Type) : Nary(Symtab, Type) {
}

Header::~Header() {
}

bool Header::implementsClass(NodeType Type) {
  return Type == kSourceHeader || Type == kReadHeader || Type == kWriteHeader;
}

Symbol* Eval::getCallName() const {
  return dyn_cast<Symbol>(getKid(0));
}

bool Eval::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  const auto* Sym = dyn_cast<Symbol>(getKid(0));
  assert(Sym);
  const auto* Defn = dyn_cast<Define>(Sym->getDefineDefinition());
  if (Defn == nullptr) {
    fprintf(error(), "Can't find define for symbol!\n");
    errorDescribeNode("In", this);
    return false;
  }
  const auto* Parms = dyn_cast<Params>(Defn->getKid(1));
  assert(Parms);
  if (int(Parms->getValue()) != getNumKids() - 1) {
    fprintf(error(), "Eval called with wrong number of arguments!\n");
    errorDescribeNode("bad eval", this);
    errorDescribeNode("called define", Defn);
    return false;
  }
  return true;
}

const Header* File::getSourceHeader(bool UseEnclosing) const {
  for (const Node* Kid : *this) {
    if (!isa<SourceHeader>(Kid))
      continue;
    return cast<SourceHeader>(Kid);
  }
  if (UseEnclosing) {
    for (SymbolTable* Sym = Symtab.getEnclosingScope().get(); Sym != nullptr;
         Sym = Sym->getEnclosingScope().get()) {
      const File* F = Sym->getInstalledRoot();
      for (const Node* Kid : *F) {
        if (!isa<SourceHeader>(Kid))
          continue;
        return cast<SourceHeader>(Kid);
      }
    }
  }
  return nullptr;
}

const Header* File::getReadHeader(bool UseEnclosing) const {
  for (const Node* Kid : *this) {
    if (!isa<ReadHeader>(Kid))
      continue;
    return cast<ReadHeader>(Kid);
  }
  if (UseEnclosing) {
    for (SymbolTable* Sym = Symtab.getEnclosingScope().get(); Sym != nullptr;
         Sym = Sym->getEnclosingScope().get()) {
      const File* F = Sym->getInstalledRoot();
      for (const Node* Kid : *F) {
        if (!isa<ReadHeader>(Kid))
          continue;
        return cast<ReadHeader>(Kid);
      }
    }
  }
  return getSourceHeader(UseEnclosing);
}

const Header* File::getWriteHeader(bool UseEnclosing) const {
  for (const Node* Kid : *this) {
    if (!isa<WriteHeader>(Kid))
      continue;
    return cast<WriteHeader>(Kid);
  }
  if (UseEnclosing) {
    for (SymbolTable* Sym = Symtab.getEnclosingScope().get(); Sym != nullptr;
         Sym = Sym->getEnclosingScope().get()) {
      const File* F = Sym->getInstalledRoot();
      for (const Node* Kid : *F) {
        if (!isa<WriteHeader>(Kid))
          continue;
        return cast<WriteHeader>(Kid);
      }
    }
  }
  return getReadHeader(UseEnclosing);
}

const Section* File::getDeclarations() const {
  for (const Node* Kid : *this)
    if (isa<Section>(Kid))
      return cast<Section>(Kid);
  assert(false && "Declarations not defined for algorithm");
  return nullptr;
}

bool File::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  if (!Parents.empty()) {
    fprintf(error(),
            "File nodes can only appear as a top-level s-expression\n");
    errorDescribeNode("Bad file node", this);
    errorDescribeContext(Parents);
    return false;
  }
  const Node* Source = nullptr;
  const Node* Read = nullptr;
  const Node* Write = nullptr;
  const Node* Decls = nullptr;
  if (hasKids()) {
    if (!isa<SourceHeader>(getKid(0))) {
      errorDescribeNode("Algorithm doesn't begin with a source header",
                        getKid(0));
      return false;
    }
  }
  for (const Node* Kid : *this) {
    switch (Kid->getType()) {
      case kSourceHeader:
        if (Source) {
          errorDescribeNode("Duplicate source header", Kid);
          errorDescribeNode("Original", Source);
          return false;
        }
        Source = Kid;
        break;
      case kReadHeader:
        if (Read) {
          errorDescribeNode("Duplicate read header", Kid);
          errorDescribeNode("Original", Source);
          return false;
        }
        Read = Kid;
        break;
      case kWriteHeader:
        if (Read) {
          errorDescribeNode("Duplicate read header", Kid);
          errorDescribeNode("Original", Source);
          return false;
        }
        Write = Kid;
        break;
      case kSection:
        if (Decls) {
          errorDescribeNode("Duplicate declarations node", Kid);
          return false;
        }
        Decls = Kid;
        break;
      default:
        errorDescribeNode("Expected header/declarations node", Kid);
        return false;
    }
  }
  if (Decls == nullptr) {
    errorDescribeNode("No (declarations) node found", this);
    return false;
  }

  return true;
}

SelectBase::~SelectBase() {
}

bool SelectBase::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NODE_DECLS) \
  case k##tag:             \
    return true;
      AST_SELECTNODE_TABLE
#undef X
  }
}

SelectBase::SelectBase(SymbolTable& Symtab, NodeType Type)
    : Nary(Symtab, Type) {
}

IntLookup* SelectBase::getIntLookup() const {
  IntLookup* Lookup = cast<IntLookup>(Symtab.getCachedValue(this));
  if (Lookup == nullptr) {
    Lookup = Symtab.create<IntLookup>();
    Symtab.setCachedValue(this, Lookup);
  }
  return Lookup;
}

const Case* SelectBase::getCase(IntType Key) const {
  IntLookup* Lookup = getIntLookup();
  return dyn_cast<Case>(Lookup->get(Key));
}

bool SelectBase::addCase(const Case* Case) const {
  return getIntLookup()->add(Case->getValue(), Case);
}

bool Case::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  // Install quick lookup to CaseBody.
  CaseBody = getKid(1);
  while (isa<Case>(CaseBody))
    CaseBody = CaseBody->getKid(1);

  // Cache value.
  Value = 0;
  const auto* CaseExp = getKid(0);
  if (const auto* LitUse = dyn_cast<LiteralUse>(CaseExp)) {
    Symbol* Sym = dyn_cast<Symbol>(LitUse->getKid(0));
    if (const auto* LitDef = Sym->getLiteralDefinition()) {
      CaseExp = LitDef->getKid(1);
    }
  }
  if (const auto* Key = dyn_cast<IntegerNode>(CaseExp)) {
    Value = Key->getValue();
  } else {
    errorDescribeNode("Case", this);
    errorDescribeNode("Case key", CaseExp);
    fprintf(error(), "Case value not found\n");
    return false;
  }

  // Install case on enclosing selector.
  for (size_t i = Parents.size(); i > 0; --i) {
    auto* Nd = Parents[i - 1];
    if (auto* Sel = dyn_cast<SelectBase>(Nd)) {
      if (Sel->addCase(this))
        return true;
      FILE* Out = error();
      fprintf(Out, "Duplicate case entries for value: %" PRIuMAX "\n",
              uintmax_t(Value));
      TextWriter Writer;
      Writer.write(Out, Sel->getCase(Value));
      fputs("vs\n", Out);
      Writer.write(Out, this);
      return false;
    }
  }
  fprintf(error(), "Case not enclosed in corresponding selector\n");
  return false;
}

namespace {

constexpr uint32_t MaxOpcodeWidth = 64;

IntType getWidthMask(uint32_t BitWidth) {
  return std::numeric_limits<IntType>::max() >> (MaxOpcodeWidth - BitWidth);
}

IntType getIntegerValue(Node* Nd) {
  if (auto* IntVal = dyn_cast<IntegerNode>(Nd))
    return IntVal->getValue();
  errorDescribeNode("Integer value expected but not found", Nd);
  return 0;
}

bool getCaseSelectorWidth(const Node* Nd, uint32_t& Width) {
  switch (Nd->getType()) {
    default:
      // Not allowed in opcode cases.
      errorDescribeNode("Non-fixed width opcode format", Nd);
      return false;
    case kBit:
      Width = 1;
      if (Width >= MaxOpcodeWidth) {
        errorDescribeNode("Bit size not valid", Nd);
        return false;
      }
      return true;
    case kUint8:
    case kUint32:
    case kUint64:
      break;
  }
  Width = getIntegerValue(Nd->getKid(0));
  if (Width == 0 || Width >= MaxOpcodeWidth) {
    errorDescribeNode("Bit size not valid", Nd);
    return false;
  }
  return true;
}

bool addFormatWidth(const Node* Nd, std::unordered_set<uint32_t>& CaseWidths) {
  uint32_t Width;
  if (!getCaseSelectorWidth(Nd, Width))
    return false;
  CaseWidths.insert(Width);
  return true;
}

bool collectCaseWidths(IntType Key,
                       const Node* Nd,
                       std::unordered_set<uint32_t>& CaseWidths) {
  switch (Nd->getType()) {
    default:
      // Not allowed in opcode cases.
      errorDescribeNode("Non-fixed width opcode format", Nd);
      return false;
    case kOpcode:
      if (isa<LastRead>(Nd->getKid(0))) {
        for (int i = 1, NumKids = Nd->getNumKids(); i < NumKids; ++i) {
          Node* Kid = Nd->getKid(i);
          assert(isa<Case>(Kid));
          const Case* C = cast<Case>(Kid);
          IntType CKey = getIntegerValue(C->getKid(0));
          const Node* Body = C->getKid(1);
          if (CKey == Key)
            // Already handled by outer case.
            continue;
          if (!collectCaseWidths(CKey, Body, CaseWidths)) {
            errorDescribeNode("Inside", Nd);
            return false;
          }
        }
      } else {
        uint32_t Width;
        if (!getCaseSelectorWidth(Nd->getKid(0), Width)) {
          errorDescribeNode("Inside", Nd);
          return false;
        }
        if (Width >= MaxOpcodeWidth) {
          errorDescribeNode("Bit width(s) too big", Nd);
          return false;
        }
        CaseWidths.insert(Width);
        for (int i = 1, NumKids = Nd->getNumKids(); i < NumKids; ++i) {
          Node* Kid = Nd->getKid(i);
          assert(isa<Case>(Kid));
          const Case* C = cast<Case>(Kid);
          IntType CKey = getIntegerValue(C->getKid(0));
          const Node* Body = C->getKid(1);
          std::unordered_set<uint32_t> LocalCaseWidths;
          if (!collectCaseWidths(CKey, Body, LocalCaseWidths)) {
            errorDescribeNode("Inside", Nd);
            return false;
          }
          for (uint32_t CaseWidth : LocalCaseWidths) {
            uint32_t CombinedWidth = Width + CaseWidth;
            if (CombinedWidth >= MaxOpcodeWidth) {
              errorDescribeNode("Bit width(s) too big", Nd);
              return false;
            }
            CaseWidths.insert(CombinedWidth);
          }
        }
      }
      return true;
    case kUint8:
    case kUint32:
    case kUint64:
      return addFormatWidth(Nd, CaseWidths);
  }
}

}  // end of anonymous namespace

#define X(tag, NODE_DECLS) \
  tag::tag(SymbolTable& Symtab) : SelectBase(Symtab, k##tag) {}
AST_SELECTNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) template tag* SymbolTable::create<tag>();
AST_SELECTNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag::~tag() {}
AST_SELECTNODE_TABLE
#undef X

Opcode::Opcode(SymbolTable& Symtab) : SelectBase(Symtab, kOpcode) {
}

template Opcode* SymbolTable::create<Opcode>();

Opcode::~Opcode() {
}

bool Opcode::validateNode(ConstNodeVectorType& Parents) const {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  CaseRangeVector.clear();

  uint32_t InitialWidth;
  if (!getCaseSelectorWidth(getKid(0), InitialWidth)) {
    errorDescribeNode("Inside", this);
    errorDescribeNode("Opcode value doesn't have fixed width", getKid(0));
    return false;
  }
  for (int i = 1, NumKids = getNumKids(); i < NumKids; ++i) {
    assert(isa<Case>(Kids[i]));
    const Case* C = cast<Case>(Kids[i]);
    std::unordered_set<uint32_t> CaseWidths;
    IntType Key = getIntegerValue(C->getKid(0));
    if (!collectCaseWidths(Key, C->getKid(1), CaseWidths)) {
      errorDescribeNode("Unable to install caches for opcode s-expression",
                        this);
      return false;
    }
    for (uint32_t NestedWidth : CaseWidths) {
      uint32_t Width = InitialWidth + NestedWidth;
      if (Width > MaxOpcodeWidth) {
        errorDescribeNode("Bit width(s) too big", this);
        return false;
      }
      IntType Min = Key << NestedWidth;
      IntType Max = Min + getWidthMask(NestedWidth);
      WriteRange Range(C, Min, Max, NestedWidth);
      CaseRangeVector.push_back(Range);
    }
  }
  // Validate that ranges are not overlapping.
  std::sort(CaseRangeVector.begin(), CaseRangeVector.end());
  for (size_t i = 0, Last = CaseRangeVector.size() - 1; i < Last; ++i) {
    const WriteRange& R1 = CaseRangeVector[i];
    const WriteRange& R2 = CaseRangeVector[i + 1];
    if (R1.getMax() >= R2.getMin()) {
      errorDescribeNode("Range 1", R1.getCase());
      errorDescribeNode("Range 2", R2.getCase());
      errorDescribeNode("Opcode case ranges not unique", this);
      return false;
    }
  }
  return true;
}

const Case* Opcode::getWriteCase(decode::IntType Value,
                                 uint32_t& SelShift,
                                 IntType& CaseMask) const {
  // TODO(kschimf): Find a faster lookup (use at least binary search).
  for (const auto& Range : CaseRangeVector) {
    if (Value < Range.getMin()) {
      SelShift = 0;
      return nullptr;
    } else if (Value <= Range.getMax()) {
      SelShift = Range.getShiftValue();
      CaseMask = getWidthMask(SelShift);
      return Range.getCase();
    }
  }
  SelShift = 0;
  return nullptr;
}

Opcode::WriteRange::WriteRange() : C(nullptr), Min(0), Max(0), ShiftValue(0) {
}

Opcode::WriteRange::WriteRange(const Case* C,
                               decode::IntType Min,
                               decode::IntType Max,
                               uint32_t ShiftValue)
    : C(C), Min(Min), Max(Max), ShiftValue(ShiftValue) {
}

Opcode::WriteRange::WriteRange(const WriteRange& R)
    : C(R.C), Min(R.Min), Max(R.Max), ShiftValue(R.ShiftValue) {
}

Opcode::WriteRange::~WriteRange() {
}

void Opcode::WriteRange::assign(const WriteRange& R) {
  C = R.C;
  Min = R.Min;
  Max = R.Max;
  ShiftValue = R.ShiftValue;
}

int Opcode::WriteRange::compare(const WriteRange& R) const {
  if (Min < R.Min)
    return -1;
  if (Min > R.Min)
    return 1;
  if (Max < R.Max)
    return -1;
  if (Max > R.Max)
    return 1;
  if ((void*)C < (void*)R.C)
    return -1;
  if ((void*)C > (void*)R.C)
    return 1;
  return 0;
}

utils::TraceClass& Opcode::WriteRange::getTrace() const {
  return C->getTrace();
}

BinaryEval::BinaryEval(SymbolTable& Symtab, Node* Encoding)
    : Unary(Symtab, kBinaryEval, Encoding) {
}

template BinaryEval* SymbolTable::create<BinaryEval>(Node* Kid);

BinaryEval::~BinaryEval() {
}

IntLookup* BinaryEval::getIntLookup() const {
  IntLookup* Lookup = cast<IntLookup>(Symtab.getCachedValue(this));
  if (Lookup == nullptr) {
    Lookup = Symtab.create<IntLookup>();
    Symtab.setCachedValue(this, Lookup);
  }
  return Lookup;
}

const Node* BinaryEval::getEncoding(IntType Value) const {
  IntLookup* Lookup = getIntLookup();
  const Node* Nd = Lookup->get(Value);
  if (Nd == nullptr)
    Nd = Symtab.getError();
  return Nd;
}

bool BinaryEval::addEncoding(const BinaryAccept* Encoding) const {
  return getIntLookup()->add(Encoding->getValue(), Encoding);
}

}  // end of namespace filt

}  // end of namespace wasm
