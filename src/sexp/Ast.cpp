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

void errorDescribeNode(const char* Message, const Node* Nd) {
  TextWriter Writer;
  FILE* Out = Nd->getErrorFile();
  if (Message)
    fprintf(Out, "%s:\n", Message);
  Writer.write(Out, Nd);
}

static const char* PredefinedName[NumPredefinedSymbols]{
    "$PredefinedSymbol::Unknown"
#define X(tag, name) , name
    PREDEFINED_SYMBOLS_TABLE
#undef X
};

bool extractIntTypeFormat(const Node* Nd, IntTypeFormat& Format) {
  Format = IntTypeFormat::Uint8;
  if (Nd == nullptr)
    return false;
  switch (Nd->getType()) {
    case OpU8Const:
      Format = IntTypeFormat::Uint8;
      return true;
    case OpI32Const:
      Format = IntTypeFormat::Varint32;
      return true;
    case OpU32Const:
      Format = IntTypeFormat::Uint32;
      return true;
    case OpI64Const:
      Format = IntTypeFormat::Varint64;
      return true;
    case OpU64Const:
      Format = IntTypeFormat::Uint64;
      return true;
    default:
      return false;
  }
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
#define X(tag, opcode, sexp_name, type_name, text_num_args, text_max_args) \
  { Op##tag, sexp_name, type_name, text_num_args, text_max_args }          \
  ,
    AST_OPCODE_TABLE
#undef X
};

const char* getNodeSexpName(NodeType Type) {
  // TODO(KarlSchimpf): Make thread safe
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < NumNodeTypes; ++i) {
      AstTraitsType& Traits = AstTraits[i];
      Mapping[int(Traits.Type)] = Traits.SexpName;
    }
  }
  char* Name = const_cast<char*>(Mapping[static_cast<int>(Type)]);
  if (Name == nullptr) {
    std::string NewName(std::string("NodeType::") +
                        std::to_string(static_cast<int>(Type)));
    Name = new char[NewName.size() + 1];
    Name[NewName.size()] = '\0';
    memcpy(Name, NewName.data(), NewName.size());
    Mapping[static_cast<int>(Type)] = Name;
  }
  return Name;
}

const char* getNodeTypeName(NodeType Type) {
  // TODO(KarlSchimpf): Make thread safe
  static std::unordered_map<int, const char*> Mapping;
  if (Mapping.empty()) {
    for (size_t i = 0; i < NumNodeTypes; ++i) {
      AstTraitsType& Traits = AstTraits[i];
      if (Traits.TypeName)
        Mapping[int(Traits.Type)] = Traits.TypeName;
    }
  }
  const char* Name = Mapping[static_cast<int>(Type)];
  if (Name == nullptr)
    Mapping[static_cast<int>(Type)] = Name = getNodeSexpName(Type);
  return Name;
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

utils::TraceClass& Node::getTrace() const {
  return Symtab.getTrace();
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

void Node::clearCaches(NodeVectorType& AdditionalNodes) {
}

void Node::installCaches(NodeVectorType& AdditionalNodes) {
}

bool Node::validateKid(NodeVectorType& Parents, Node* Kid) {
  Parents.push_back(this);
  bool Result = Kid->validateSubtree(Parents);
  Parents.pop_back();
  return Result;
}

bool Node::validateKids(NodeVectorType& Parents) {
  if (!hasKids())
    return true;
  TRACE(int, "NumKids", getNumKids());
  for (auto* Kid : *this)
    if (!validateKid(Parents, Kid))
      return false;
  return true;
}

bool Node::validateSubtree(NodeVectorType& Parents) {
  TRACE_METHOD("validateSubtree");
  TRACE(node_ptr, nullptr, this);
  if (!validateNode(Parents))
    return false;
  return validateKids(Parents);
}

bool Node::validateNode(NodeVectorType& Scope) {
  return true;
}

IntLookupNode::IntLookupNode(SymbolTable& Symtab)
    : NullaryNode(Symtab, OpIntLookup) {
}

IntLookupNode::~IntLookupNode() {
}

const Node* IntLookupNode::get(decode::IntType Value) const {
  if (Lookup.count(Value))
    return Lookup[Value];
  return nullptr;
}

bool IntLookupNode::add(decode::IntType Value, const Node* Nd) {
  if (Lookup.count(Value))
    return false;
  Lookup[Value] = Nd;
  return true;
}

SymbolDefnNode::SymbolDefnNode(SymbolTable& Symtab)
    : NullaryNode(Symtab, OpSymbolDefn),
      Symbol(nullptr),
      DefineDefinition(nullptr),
      LiteralDefinition(nullptr) {
}

SymbolDefnNode::~SymbolDefnNode() {
}

const std::string& SymbolDefnNode::getName() const {
  return Symbol->getName();
}

SymbolNode::SymbolNode(SymbolTable& Symtab, const std::string& Name)
    : NullaryNode(Symtab, OpSymbol), Name(Name) {
  init();
}

SymbolNode::~SymbolNode() {
}

void SymbolNode::init() {
  PredefinedValue = PredefinedSymbol::Unknown;
}

SymbolDefnNode* SymbolNode::getSymbolDefn() const {
  SymbolDefnNode* Defn = cast<SymbolDefnNode>(Symtab.getCachedValue(this));
  if (Defn == nullptr) {
    Defn = Symtab.create<SymbolDefnNode>();
    Defn->setSymbol(this);
    Symtab.setCachedValue(this, Defn);
  }
  return Defn;
}

void SymbolNode::setPredefinedSymbol(PredefinedSymbol NewValue) {
  if (PredefinedValue != PredefinedSymbol::Unknown)
    fatal(std::string("Can't define \"") + filt::getName(PredefinedValue) +
          " and " + filt::getName(NewValue));
  PredefinedValue = NewValue;
}

// Note: we create duumy virtual forceCompilation() to force legal
// class definitions to be compiled in here. Classes not explicitly
// instantiated here will not link. This used to force an error if
// a node type is not defined with the correct template class.

void SymbolNode::clearCaches(NodeVectorType& AdditionalNodes) {
  const Node* DefineDefinition = getDefineDefinition();
  if (DefineDefinition)
    AdditionalNodes.push_back(const_cast<Node*>(DefineDefinition));
}

void SymbolNode::installCaches(NodeVectorType& AdditionalNodes) {
  const Node* DefineDefinition = getDefineDefinition();
  if (DefineDefinition)
    AdditionalNodes.push_back(const_cast<Node*>(DefineDefinition));
}

SymbolTable::SymbolTable(std::shared_ptr<SymbolTable> EnclosingScope)
    : EnclosingScope(EnclosingScope) {
}

SymbolTable::SymbolTable() {
  init();
}

SymbolTable::~SymbolTable() {
  clear();
  deallocateNodes();
}

FILE* SymbolTable::getErrorFile() const {
  return stderr;
}

FILE* SymbolTable::error() const {
  FILE* Out = getErrorFile();
  fputs("Error: ", Out);
  return Out;
}

SymbolNode* SymbolTable::getSymbol(const std::string& Name) {
  return SymbolMap[Name];
}

void SymbolTable::clear() {
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

void SymbolTable::init() {
  Root = nullptr;
  TargetHeader = nullptr;
  NextCreationIndex = 0;
  Error = create<ErrorNode>();
  Predefined.reserve(NumPredefinedSymbols);
  for (size_t i = 0; i < NumPredefinedSymbols; ++i) {
    SymbolNode* Nd = getSymbolDefinition(PredefinedName[i]);
    Predefined.push_back(Nd);
    Nd->setPredefinedSymbol(toPredefinedSymbol(i));
  }
  BlockEnterCallback =
      create<CallbackNode>(Predefined[uint32_t(PredefinedSymbol::Block_enter)]);
  BlockExitCallback =
      create<CallbackNode>(Predefined[uint32_t(PredefinedSymbol::Block_exit)]);
}

SymbolNode* SymbolTable::getSymbolDefinition(const std::string& Name) {
  SymbolNode* Node = SymbolMap[Name];
  if (Node == nullptr) {
    Node = new SymbolNode(*this, Name);
    Allocated.push_back(Node);
    SymbolMap[Name] = Node;
  }
  return Node;
}

#define X(tag, format, defval, mergable, NODE_DECLS)                 \
  tag##Node* SymbolTable::get##tag##Definition(IntType Value,        \
                                               ValueFormat Format) { \
    if (mergable) {                                                  \
      IntegerValue I(Op##tag, Value, Format, false);                 \
      IntegerNode* Node = IntMap[I];                                 \
      if (Node == nullptr) {                                         \
        Node = new tag##Node(*this, Value, Format);                  \
        Allocated.push_back(Node);                                   \
        IntMap[I] = Node;                                            \
      }                                                              \
      return dyn_cast<tag##Node>(Node);                              \
    }                                                                \
    tag##Node* Node = new tag##Node(*this, Value, Format);           \
    Allocated.push_back(Node);                                       \
    return Node;                                                     \
  }                                                                  \
  tag##Node* SymbolTable::get##tag##Definition() {                   \
    if (mergable) {                                                  \
      IntegerValue I(Op##tag, (defval), ValueFormat::Decimal, true); \
      IntegerNode* Node = IntMap[I];                                 \
      if (Node == nullptr) {                                         \
        Node = new tag##Node(*this);                                 \
        Allocated.push_back(Node);                                   \
        IntMap[I] = Node;                                            \
      }                                                              \
      return dyn_cast<tag##Node>(Node);                              \
    }                                                                \
    tag##Node* Node = new tag##Node(*this);                          \
    Allocated.push_back(Node);                                       \
    return Node;                                                     \
  }
AST_INTEGERNODE_TABLE
#undef X

void SymbolTable::install(Node* Root) {
  TRACE_METHOD("install");
  CachedValue.clear();
  this->Root = Root;
  // Before starting, clear all known caches.
  VisitedNodesType VisitedNodes;
  NodeVectorType AdditionalNodes;
  for (const auto& Pair : SymbolMap)
    clearSubtreeCaches(Pair.second, VisitedNodes, AdditionalNodes);
  clearSubtreeCaches(Root, VisitedNodes, AdditionalNodes);
  NodeVectorType MoreNodes;
  while (!AdditionalNodes.empty()) {
    MoreNodes.swap(AdditionalNodes);
    while (!MoreNodes.empty()) {
      Node* Nd = MoreNodes.back();
      MoreNodes.pop_back();
      clearSubtreeCaches(Nd, VisitedNodes, AdditionalNodes);
    }
  }
  VisitedNodes.clear();
  installDefinitions(Root);
  std::vector<Node*> Parents;
  if (!Root->validateSubtree(Parents))
    fatal("Unable to install algorthms, validation failed!");
  for (const auto& Pair : SymbolMap)
    installSubtreeCaches(Pair.second, VisitedNodes, AdditionalNodes);
  while (!AdditionalNodes.empty()) {
    MoreNodes.swap(AdditionalNodes);
    while (!MoreNodes.empty()) {
      Node* Nd = MoreNodes.back();
      MoreNodes.pop_back();
      installSubtreeCaches(Nd, VisitedNodes, AdditionalNodes);
    }
  }
}

void SymbolTable::clearSubtreeCaches(Node* Nd,
                                     VisitedNodesType& VisitedNodes,
                                     NodeVectorType& AdditionalNodes) {
  if (VisitedNodes.count(Nd))
    return;
  TRACE_METHOD("clearSubtreeCaches");
  TRACE(node_ptr, nullptr, Nd);
  VisitedNodes.insert(Nd);
  Nd->clearCaches(AdditionalNodes);
  for (auto* Kid : *Nd)
    AdditionalNodes.push_back(Kid);
}

void SymbolTable::installSubtreeCaches(Node* Nd,
                                       VisitedNodesType& VisitedNodes,
                                       NodeVectorType& AdditionalNodes) {
  if (VisitedNodes.count(Nd))
    return;
  TRACE_METHOD("installSubtreeCaches");
  TRACE(node_ptr, nullptr, Nd);
  VisitedNodes.insert(Nd);
  Nd->installCaches(AdditionalNodes);
  for (auto* Kid : *Nd)
    AdditionalNodes.push_back(Kid);
}

const FileHeaderNode* SymbolTable::getSourceHeader() const {
  if (Root == nullptr)
    return nullptr;
  return dyn_cast<FileHeaderNode>(Root->getKid(0));
}

void SymbolTable::installDefinitions(Node* Root) {
  TRACE_METHOD("installDefinitions");
  TRACE(node_ptr, nullptr, Root);
  if (Root == nullptr)
    return;
  switch (Root->getType()) {
    default:
      return;
    case OpFile:
      if (TargetHeader == nullptr) {
        TargetHeader = dyn_cast<FileHeaderNode>(Root->getKid(1));
        if (TargetHeader == nullptr)
          TargetHeader = getSourceHeader();
      }
    // intentionally fall to next case.
    case OpSection:
      for (Node* Kid : *Root)
        installDefinitions(Kid);
      return;
    case OpDefine: {
      if (auto* DefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        DefineSymbol->setDefineDefinition(Root);
        return;
      }
      errorDescribeNode("Malformed define", Root);
      fatal("Malformed define s-expression found!");
      return;
    }
    case OpLiteralDef: {
      if (auto* LiteralSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        LiteralSymbol->setLiteralDefinition(Root);
        return;
      }
      errorDescribeNode("Malformed", Root);
      fatal("Malformed literal s-expression found!");
      return;
    }
    case OpRename: {
      if (auto* OldSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        if (auto* NewSymbol = dyn_cast<SymbolNode>(Root->getKid(1))) {
          Node* Defn = const_cast<Node*>(OldSymbol->getDefineDefinition());
          NewSymbol->setDefineDefinition(Defn);
          return;
        }
      }
      errorDescribeNode("Malformed", Root);
      fatal("Malformed rename s-expression found!");
      return;
    }
    case OpUndefine: {
      if (auto* UndefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        UndefineSymbol->setDefineDefinition(nullptr);
        return;
      }
      errorDescribeNode("Can't undefine", Root);
      fatal("Malformed undefine s-expression found!");
    }
  }
}

TraceClass& SymbolTable::getTrace() {
  return *getTracePtr();
}

void SymbolTable::describe(FILE* Out) {
  TextWriter Writer;
  Writer.write(Out, getInstalledRoot());
}

void SymbolTable::stripCallbacksExcept(std::set<std::string>& KeepActions) {
  install(stripCallbacksExcept(KeepActions, Root));
}

Node* SymbolTable::stripUsing(Node* Root,
                              std::function<Node*(Node*)> stripKid) {
  switch (Root->getType()) {
    default:
      for (int i = 0; i < Root->getNumKids(); ++i)
        Root->setKid(i, stripKid(Root->getKid(i)));
      return Root;
#define X(tag, NODE_DECLS) case Op##tag:
      AST_NARYNODE_TABLE
#undef X
      {
        std::vector<Node*> Kids;
        for (int i = 0; i < Root->getNumKids(); ++i) {
          Node* Kid = stripKid(Root->getKid(i));
          if (!isa<VoidNode>(Kid))
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
        if (Kids.size() == 1)
          return Kids[0];
        NaryNode* Nd = dyn_cast<NaryNode>(Root);
        if (Nd == nullptr)
          break;
        Nd->clearKids();
        for (auto Kid : Kids)
          Nd->append(Kid);
        return Nd;
      }
  }
  return create<VoidNode>();
}

Node* SymbolTable::stripCallbacksExcept(std::set<std::string>& KeepActions,
                                        Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(Root, [&](Node* Nd) -> Node* {
        return stripCallbacksExcept(KeepActions, Nd);
      });
    case OpCallback: {
      auto* Sym = dyn_cast<SymbolNode>(Root->getKid(0));
      if (Sym == nullptr)
        return Root;
      if (KeepActions.count(Sym->getName()))
        return Root;
      break;
    }
  }
  return create<VoidNode>();
}

void SymbolTable::stripLiterals() {
  install(stripLiteralDefs(stripLiteralUses(Root)));
}

Node* SymbolTable::stripLiteralUses(Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(
          Root, [&](Node* Nd) -> Node* { return stripLiteralUses(Nd); });
    case OpLiteralUse: {
      const auto* Use = cast<LiteralUseNode>(Root);
      const auto* Sym = dyn_cast<SymbolNode>(Use->getKid(0));
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
  return create<VoidNode>();
}

Node* SymbolTable::stripLiteralDefs(Node* Root) {
  switch (Root->getType()) {
    default:
      return stripUsing(
          Root, [&](Node* Nd) -> Node* { return stripLiteralDefs(Nd); });
    case OpLiteralDef:
      break;
  }
  return create<VoidNode>();
}

NullaryNode::NullaryNode(SymbolTable& Symtab, NodeType Type)
    : Node(Symtab, Type) {
}

NullaryNode::~NullaryNode() {
}

int NullaryNode::getNumKids() const {
  return 0;
}

Node* NullaryNode::getKid(int) const {
  return nullptr;
}

void NullaryNode::setKid(int, Node*) {
  decode::fatal("NullaryNode::setKid not allowed");
}

bool NullaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_NULLARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS) \
  tag##Node::tag##Node(SymbolTable& Symtab) : NullaryNode(Symtab, Op##tag) {}
AST_NULLARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) template tag##Node* SymbolTable::create<tag##Node>();
AST_NULLARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_NULLARYNODE_TABLE
#undef X

UnaryNode::UnaryNode(SymbolTable& Symtab, NodeType Type, Node* Kid)
    : Node(Symtab, Type) {
  Kids[0] = Kid;
}

UnaryNode::~UnaryNode() {
}

int UnaryNode::getNumKids() const {
  return 1;
}

Node* UnaryNode::getKid(int Index) const {
  if (Index < 1)
    return Kids[0];
  return nullptr;
}

void UnaryNode::setKid(int Index, Node* NewValue) {
  assert(Index < 1);
  Kids[0] = NewValue;
}

bool UnaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
    case OpBinaryEval:
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_UNARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS)                             \
  tag##Node::tag##Node(SymbolTable& Symtab, Node* Kid) \
      : UnaryNode(Symtab, Op##tag, Kid) {}
AST_UNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  template tag##Node* SymbolTable::create<tag##Node>(Node * Nd);
AST_UNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_UNARYNODE_TABLE
#undef X

IntegerNode::IntegerNode(SymbolTable& Symtab,
                         NodeType Type,
                         decode::IntType Value,
                         decode::ValueFormat Format,
                         bool isDefault)
    : NullaryNode(Symtab, Type), Value(Type, Value, Format, isDefault) {
}

IntegerNode::~IntegerNode() {
}

bool IntegerNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
    case OpBinaryAccept:
#define X(tag, format, defval, mergable, NODE_DECLS) case Op##tag:
      AST_INTEGERNODE_TABLE
#undef X
      return true;
  }
}

#define X(tag, format, defval, mergable, NODE_DECLS)               \
  tag##Node::tag##Node(SymbolTable& Symtab, decode::IntType Value, \
                       decode::ValueFormat Format)                 \
      : IntegerNode(Symtab, Op##tag, Value, Format, false) {}
AST_INTEGERNODE_TABLE
#undef X

#define X(tag, format, defval, mergable, NODE_DECLS)                         \
  tag##Node::tag##Node(SymbolTable& Symtab)                                  \
      : IntegerNode(Symtab, Op##tag, (defval), decode::ValueFormat::Decimal, \
                    true) {}
AST_INTEGERNODE_TABLE
#undef X

#define X(tag, format, defval, mergable, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_INTEGERNODE_TABLE
#undef X

bool ParamNode::validateNode(NodeVectorType& Parents) {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  for (size_t i = Parents.size(); i > 0; --i) {
    auto* Nd = Parents[i - 1];
    auto* Define = dyn_cast<DefineNode>(Nd);
    if (Define == nullptr) {
      TRACE(node_ptr, "parent Nd", Nd);
      continue;
    }
    TRACE(node_ptr, "Enclosing define", Nd);
    // Scope found. Check if parameter is legal.
    if (!Define->isValidParam(getValue())) {
      FILE* Out = error();
      fputs("Param ", Out);
      fprint_IntType(Out, getValue());
      fprintf(Out, " not defined for method: %s\n", Define->getName().c_str());
      return false;
    }
    return true;
  }
  return false;
}

BinaryAcceptNode::BinaryAcceptNode(SymbolTable& Symtab)
    : IntegerNode(Symtab,
                  OpBinaryAccept,
                  0,
                  decode::ValueFormat::Hexidecimal,
                  true) {
}

BinaryAcceptNode::BinaryAcceptNode(SymbolTable& Symtab,
                                   decode::IntType Value,
                                   unsigned NumBits)
    : IntegerNode(Symtab,
                  OpBinaryAccept,
                  Value,
                  decode::ValueFormat::Hexidecimal,
                  NumBits) {
}

BinaryAcceptNode* SymbolTable::createBinaryAccept(IntType Value,
                                                  unsigned NumBits) {
  BinaryAcceptNode* Nd = new BinaryAcceptNode(*this, Value, NumBits);
  Allocated.push_back(Nd);
  return Nd;
}

template BinaryAcceptNode* SymbolTable::create<BinaryAcceptNode>();

BinaryAcceptNode::~BinaryAcceptNode() {
}

bool BinaryAcceptNode::validateNode(NodeVectorType& Parents) {
  // Defines path (value) from leaf to (binary) root node, guaranteeing each
  // accept node has a unique value that can be case selected.
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);
  IntType MyValue = 0;
  unsigned MyNumBits = 0;
  Node* LastNode = this;
  for (size_t i = Parents.size(); i > 0; --i) {
    Node* Nd = Parents[i - 1];
    switch (Nd->getType()) {
      case OpBinaryEval: {
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
        if (!cast<BinaryEvalNode>(Nd)->addEncoding(this)) {
          fprintf(error(), "Can't install opcode, malformed: %s\n", getName());
          Success = false;
        }
        return Success;
      }
      case OpBinarySelect:
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
                getNodeSexpName(NodeType::OpBinaryEval));
        fprintf(Out, "Appears in:\n");
        Writer.write(Out, Nd);
        return false;
      }
    }
  }
  fprintf(error(), "%s can't appear at top level\n", getName());
  return false;
}

BinaryNode::BinaryNode(SymbolTable& Symtab,
                       NodeType Type,
                       Node* Kid1,
                       Node* Kid2)
    : Node(Symtab, Type) {
  Kids[0] = Kid1;
  Kids[1] = Kid2;
}

BinaryNode::~BinaryNode() {
}

int BinaryNode::getNumKids() const {
  return 2;
}

Node* BinaryNode::getKid(int Index) const {
  if (Index < 2)
    return Kids[Index];
  return nullptr;
}

void BinaryNode::setKid(int Index, Node* NewValue) {
  assert(Index < 2);
  Kids[Index] = NewValue;
}

bool BinaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_BINARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS)                                          \
  tag##Node::tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2) \
      : BinaryNode(Symtab, Op##tag, Kid1, Kid2) {}
AST_BINARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  template tag##Node* SymbolTable::create<tag##Node>(Node * Nd1, Node * Nd2);
AST_BINARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_BINARYNODE_TABLE
#undef X

TernaryNode::TernaryNode(SymbolTable& Symtab,
                         NodeType Type,
                         Node* Kid1,
                         Node* Kid2,
                         Node* Kid3)
    : Node(Symtab, Type) {
  Kids[0] = Kid1;
  Kids[1] = Kid2;
  Kids[2] = Kid3;
}

TernaryNode::~TernaryNode() {
}

int TernaryNode::getNumKids() const {
  return 3;
}

Node* TernaryNode::getKid(int Index) const {
  if (Index < 3)
    return Kids[Index];
  return nullptr;
}

void TernaryNode::setKid(int Index, Node* NewValue) {
  assert(Index < 3);
  Kids[Index] = NewValue;
}

bool TernaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_TERNARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS)                                          \
  tag##Node::tag##Node(SymbolTable& Symtab, Node* Kid1, Node* Kid2, \
                       Node* Kid3)                                  \
      : TernaryNode(Symtab, Op##tag, Kid1, Kid2, Kid3) {}
AST_TERNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS)                                                   \
  template tag##Node* SymbolTable::create<tag##Node>(Node * Nd1, Node * Nd2, \
                                                     Node * Nd3);
AST_TERNARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_TERNARYNODE_TABLE
#undef X

// Returns nullptr if P is illegal, based on the define.
bool DefineNode::isValidParam(IntType Index) {
  assert(isa<ParamsNode>(getKid(1)));
  return Index < cast<ParamsNode>(getKid(1))->getValue();
}

const std::string DefineNode::getName() const {
  assert(getNumKids() >= 3);
  assert(isa<SymbolNode>(getKid(0)));
  return cast<SymbolNode>(getKid(0))->getName();
}

size_t DefineNode::getNumLocals() const {
  assert(getNumKids() >= 3);
  if (auto* Locals = dyn_cast<LocalsNode>(getKid(2)))
    return Locals->getValue();
  return 0;
}

Node* DefineNode::getBody() const {
  assert(getNumKids() >= 3);
  Node* Nd = getKid(2);
  if (isa<LocalsNode>(Nd)) {
    assert(getNumKids() >= 4);
    return getKid(3);
  }
  return Nd;
}

NaryNode::NaryNode(SymbolTable& Symtab, NodeType Type) : Node(Symtab, Type) {
}

NaryNode::~NaryNode() {
}

int NaryNode::getNumKids() const {
  return Kids.size();
}

Node* NaryNode::getKid(int Index) const {
  return Kids[Index];
}

void NaryNode::setKid(int Index, Node* N) {
  Kids[Index] = N;
}

void NaryNode::clearKids() {
  Kids.clear();
}

void NaryNode::append(Node* Kid) {
  Kids.emplace_back(Kid);
}

bool NaryNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NOD_DECLS) \
  case Op##tag:           \
    return true;
      AST_NARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS) \
  tag##Node::tag##Node(SymbolTable& Symtab) : NaryNode(Symtab, Op##tag) {}
AST_NARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) template tag##Node* SymbolTable::create<tag##Node>();
AST_NARYNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_NARYNODE_TABLE
#undef X

SymbolNode* EvalNode::getCallName() const {
  return dyn_cast<SymbolNode>(getKid(0));
}

bool EvalNode::validateNode(NodeVectorType& Parents) {
  const auto* Sym = dyn_cast<SymbolNode>(getKid(0));
  assert(Sym);
  const auto* Defn = dyn_cast<DefineNode>(Sym->getDefineDefinition());
  if (Defn == nullptr) {
    fprintf(error(), "Can't find define for symbol!\n");
    errorDescribeNode("In", this);
    return false;
  }
  const auto* Params = dyn_cast<ParamsNode>(Defn->getKid(1));
  assert(Params);
  if (int(Params->getValue()) != getNumKids() - 1) {
    fprintf(error(), "Eval called with wrong number of arguments!\n");
    errorDescribeNode("bad eval", this);
    errorDescribeNode("called define", Defn);
    return false;
  }
  return true;
}

SelectBaseNode::~SelectBaseNode() {
}

bool SelectBaseNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_SELECTNODE_TABLE
#undef X
  }
}

SelectBaseNode::SelectBaseNode(SymbolTable& Symtab, NodeType Type)
    : NaryNode(Symtab, Type) {
}

IntLookupNode* SelectBaseNode::getIntLookup() const {
  IntLookupNode* Lookup = cast<IntLookupNode>(Symtab.getCachedValue(this));
  if (Lookup == nullptr) {
    Lookup = Symtab.create<IntLookupNode>();
    Symtab.setCachedValue(this, Lookup);
  }
  return Lookup;
}

const CaseNode* SelectBaseNode::getCase(IntType Key) const {
  IntLookupNode* Lookup = getIntLookup();
  if (const CaseNode* Case = dyn_cast<CaseNode>(Lookup->get(Key)))
    return Case;
  return nullptr;
}

bool SelectBaseNode::addCase(const CaseNode* Case) {
  return getIntLookup()->add(Case->getValue(), Case);
}

bool CaseNode::validateNode(NodeVectorType& Parents) {
  TRACE_METHOD("validateNode");
  TRACE(node_ptr, nullptr, this);

  {  // Cache value.
    Value = 0;
    const auto* CaseExp = getKid(0);
    if (const auto* LitUse = dyn_cast<LiteralUseNode>(CaseExp)) {
      SymbolNode* Sym = dyn_cast<SymbolNode>(LitUse->getKid(0));
      if (const auto* LitDef = Sym->getLiteralDefinition()) {
        CaseExp = LitDef->getKid(1);
      }
    }
    if (const auto* Key = dyn_cast<IntegerNode>(CaseExp)) {
      Value = Key->getValue();
    } else {
      fprintf(error(), "Case value not found\n");
      return false;
    }
  }

  // Install case on enclosing selector.
  for (size_t i = Parents.size(); i > 0; --i) {
    auto* Nd = Parents[i - 1];
    if (auto* Sel = dyn_cast<SelectBaseNode>(Nd)) {
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
    case OpBit:
      Width = 1;
      if (Width >= MaxOpcodeWidth) {
        errorDescribeNode("Bit size not valid", Nd);
        return false;
      }
      return true;
    case OpUint8:
    case OpUint32:
    case OpUint64:
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
    case OpOpcode:
      if (isa<LastReadNode>(Nd->getKid(0))) {
        for (int i = 1, NumKids = Nd->getNumKids(); i < NumKids; ++i) {
          Node* Kid = Nd->getKid(i);
          assert(isa<CaseNode>(Kid));
          const CaseNode* Case = cast<CaseNode>(Kid);
          IntType CaseKey = getIntegerValue(Case->getKid(0));
          const Node* CaseBody = Case->getKid(1);
          if (CaseKey == Key)
            // Already handled by outer case.
            continue;
          if (!collectCaseWidths(CaseKey, CaseBody, CaseWidths)) {
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
          assert(isa<CaseNode>(Kid));
          const CaseNode* Case = cast<CaseNode>(Kid);
          IntType CaseKey = getIntegerValue(Case->getKid(0));
          const Node* CaseBody = Case->getKid(1);
          std::unordered_set<uint32_t> LocalCaseWidths;
          if (!collectCaseWidths(CaseKey, CaseBody, LocalCaseWidths)) {
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
    case OpUint8:
    case OpUint32:
    case OpUint64:
      return addFormatWidth(Nd, CaseWidths);
  }
}

}  // end of anonymous namespace

#define X(tag, NODE_DECLS) \
  tag##Node::tag##Node(SymbolTable& Symtab) : SelectBaseNode(Symtab, Op##tag) {}
AST_SELECTNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) template tag##Node* SymbolTable::create<tag##Node>();
AST_SELECTNODE_TABLE
#undef X

#define X(tag, NODE_DECLS) \
  tag##Node::~tag##Node() {}
AST_SELECTNODE_TABLE
#undef X

OpcodeNode::OpcodeNode(SymbolTable& Symtab) : SelectBaseNode(Symtab, OpOpcode) {
}

template OpcodeNode* SymbolTable::create<OpcodeNode>();

OpcodeNode::~OpcodeNode() {
}

bool OpcodeNode::validateNode(NodeVectorType& Parents) {
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
    assert(isa<CaseNode>(Kids[i]));
    const CaseNode* Case = cast<CaseNode>(Kids[i]);
    std::unordered_set<uint32_t> CaseWidths;
    IntType Key = getIntegerValue(Case->getKid(0));
    if (!collectCaseWidths(Key, Case->getKid(1), CaseWidths)) {
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
      WriteRange Range(Case, Min, Max, NestedWidth);
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

const CaseNode* OpcodeNode::getWriteCase(decode::IntType Value,
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

OpcodeNode::WriteRange::WriteRange()
    : Case(nullptr), Min(0), Max(0), ShiftValue(0) {
}

OpcodeNode::WriteRange::WriteRange(const CaseNode* Case,
                                   decode::IntType Min,
                                   decode::IntType Max,
                                   uint32_t ShiftValue)
    : Case(Case), Min(Min), Max(Max), ShiftValue(ShiftValue) {
}

OpcodeNode::WriteRange::WriteRange(const WriteRange& R)
    : Case(R.Case), Min(R.Min), Max(R.Max), ShiftValue(R.ShiftValue) {
}

OpcodeNode::WriteRange::~WriteRange() {
}

void OpcodeNode::WriteRange::assign(const WriteRange& R) {
  Case = R.Case;
  Min = R.Min;
  Max = R.Max;
  ShiftValue = R.ShiftValue;
}

int OpcodeNode::WriteRange::compare(const WriteRange& R) const {
  if (Min < R.Min)
    return -1;
  if (Min > R.Min)
    return 1;
  if (Max < R.Max)
    return -1;
  if (Max > R.Max)
    return 1;
  if ((void*)Case < (void*)R.Case)
    return -1;
  if ((void*)Case > (void*)R.Case)
    return 1;
  return 0;
}

utils::TraceClass& OpcodeNode::WriteRange::getTrace() const {
  return Case->getTrace();
}

BinaryEvalNode::BinaryEvalNode(SymbolTable& Symtab, Node* Encoding)
    : UnaryNode(Symtab, OpBinaryEval, Encoding) {
}

template BinaryEvalNode* SymbolTable::create<BinaryEvalNode>(Node* Kid);

BinaryEvalNode::~BinaryEvalNode() {
}

IntLookupNode* BinaryEvalNode::getIntLookup() const {
  IntLookupNode* Lookup = cast<IntLookupNode>(Symtab.getCachedValue(this));
  if (Lookup == nullptr) {
    Lookup = Symtab.create<IntLookupNode>();
    Symtab.setCachedValue(this, Lookup);
  }
  return Lookup;
}

const Node* BinaryEvalNode::getEncoding(IntType Value) const {
  IntLookupNode* Lookup = getIntLookup();
  const Node* Nd = Lookup->get(Value);
  if (Nd == nullptr)
    Nd = Symtab.getError();
  return Nd;
}

bool BinaryEvalNode::addEncoding(BinaryAcceptNode* Encoding) {
  return getIntLookup()->add(Encoding->getValue(), Encoding);
}

}  // end of namespace filt

}  // end of namespace wasm
