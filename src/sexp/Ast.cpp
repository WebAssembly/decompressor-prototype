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

#include "interp/IntFormats.h"
#include "sexp/TextWriter.h"
#include "utils/Defs.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

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
}

namespace filt {

namespace {

void describeNode(const char* Message, const Node* Nd) {
  TextWriter Writer;
  fprintf(stderr, "%s:\n", Message);
  Writer.write(stderr, Nd);
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

bool Node::definesIntTypeFormat() const {
  IntTypeFormat Format;
  return extractIntTypeFormat(this, Format);
}

IntTypeFormat Node::getIntTypeFormat() const {
  IntTypeFormat Format;
  extractIntTypeFormat(this, Format);
  return Format;
}

void IntegerValue::describe(FILE* Out) const {
  fprintf(Out, "%s<", getNodeSexpName(Type));
  writeInt(Out, Value, Format);
  fprintf(Out, ", %s>", getName(Format));
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

bool Node::validateSubtree(NodeVectorType& Parents) {
  if (!validateNode(Parents))
    return false;
  if (hasKids()) {
    Parents.push_back(this);
    TRACE(int, "NumKids", getNumKids());
    for (auto* Kid : *this)
      if (!Kid->validateSubtree(Parents))
        return false;
    Parents.pop_back();
  }
  return true;
}

bool Node::validateNode(NodeVectorType& Scope) {
  return true;
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
  if (DefineDefinition)
    AdditionalNodes.push_back(DefineDefinition);
  if (LiteralDefinition)
    AdditionalNodes.push_back(LiteralDefinition);
}

void SymbolNode::installCaches(NodeVectorType& AdditionalNodes) {
  if (DefineDefinition)
    AdditionalNodes.push_back(DefineDefinition);
  if (LiteralDefinition)
    AdditionalNodes.push_back(LiteralDefinition);
}

SymbolTable::SymbolTable()
    // TODO(karlschimpf) Switch Alloc to an ArenaAllocator once working.
    // TODO(karlschimpf) Figure out why we can't deallocate Allocated!
    : Allocated(new std::vector<Node*>()),
#if 0
      Trace("sexp_st"),
#endif
      NextCreationIndex(0),
      Predefined(new std::vector<SymbolNode*>()) {
  Error = create<ErrorNode>();
  init();
}

SymbolTable::~SymbolTable() {
  clear();
  deallocateNodes();
}

const FileHeaderNode* SymbolTable::getInstalledFileHeader() const {
  if (Root)
    if (const auto* File = dyn_cast<FileNode>(Root))
      if (const auto* Header = dyn_cast<FileHeaderNode>(File->getKid(0)))
        return Header;
  return nullptr;
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
  for (Node* Nd : *Allocated)
    delete Nd;
}

void SymbolTable::init() {
  Predefined->reserve(NumPredefinedSymbols);
  for (size_t i = 0; i < NumPredefinedSymbols; ++i) {
    SymbolNode* Nd = getSymbolDefinition(PredefinedName[i]);
    Predefined->push_back(Nd);
    Nd->setPredefinedSymbol(toPredefinedSymbol(i));
  }
  BlockEnterCallback = create<CallbackNode>(
      (*Predefined)[uint32_t(PredefinedSymbol::Block_enter)]);
  BlockExitCallback = create<CallbackNode>(
      (*Predefined)[uint32_t(PredefinedSymbol::Block_exit)]);
}

SymbolNode* SymbolTable::getSymbolDefinition(const std::string& Name) {
  SymbolNode* Node = SymbolMap[Name];
  if (Node == nullptr) {
    Node = create<SymbolNode>(Name);
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
        Node = create<tag##Node>(Value, Format);                     \
        IntMap[I] = Node;                                            \
      }                                                              \
      return dyn_cast<tag##Node>(Node);                              \
    }                                                                \
    return create<tag##Node>(Value, Format);                         \
  }                                                                  \
  tag##Node* SymbolTable::get##tag##Definition() {                   \
    if (mergable) {                                                  \
      IntegerValue I(Op##tag, (defval), ValueFormat::Decimal, true); \
      IntegerNode* Node = IntMap[I];                                 \
      if (Node == nullptr) {                                         \
        Node = create<tag##Node>();                                  \
        IntMap[I] = Node;                                            \
      }                                                              \
      return dyn_cast<tag##Node>(Node);                              \
    }                                                                \
    return create<tag##Node>();                                      \
  }
AST_INTEGERNODE_TABLE
#undef X

void SymbolTable::install(Node* Root) {
  TRACE_METHOD("install");
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

void SymbolTable::installDefinitions(Node* Root) {
  TRACE_METHOD("installDefinitions");
  TRACE(node_ptr, nullptr, Root);
  if (Root == nullptr)
    return;
  switch (Root->getType()) {
    default:
      return;
    case OpFile:
    case OpSection:
      for (Node* Kid : *Root)
        installDefinitions(Kid);
      return;
    case OpDefine: {
      if (auto* DefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        DefineSymbol->setDefineDefinition(Root);
        return;
      }
      describeNode("Malformed", Root);
      fatal("Malformed define s-expression found!");
      return;
    }
    case OpLiteralDef: {
      if (auto* LiteralSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        LiteralSymbol->setLiteralDefinition(Root);
        return;
      }
      describeNode("Malformed", Root);
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
      describeNode("Malformed", Root);
      fatal("Malformed rename s-expression found!");
      return;
    }
    case OpUndefine: {
      if (auto* UndefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        UndefineSymbol->setDefineDefinition(nullptr);
        return;
      }
      describeNode("Can't undefine", Root);
      fatal("Malformed undefine s-expression found!");
    }
  }
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
  void tag##Node::forceCompilation() {}
AST_NULLARYNODE_TABLE
#undef X

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
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_UNARYNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
AST_UNARYNODE_TABLE
#undef X

bool IntegerNode::implementsClass(NodeType Type) {
  switch (Type) {
    default:
      return false;
#define X(tag, format, defval, mergable, NODE_DECLS) \
  case Op##tag:                                      \
    return true;
      AST_INTEGERNODE_TABLE
#undef X
  }
}

#define X(tag, format, defval, mergable, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
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
      FILE* Out = getTrace().getFile();
      fputs("Error: Param ", Out);
      fprint_IntType(Out, getValue());
      fprintf(Out, " not defined for method: %s\n", Define->getName().c_str());
      return false;
    }
    return true;
  }
  return false;
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

#define X(tag, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
AST_BINARYNODE_TABLE
#undef X

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

#define X(tag, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
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

void NaryNode::append(Node* Kid) {
  Kids.emplace_back(Kid);
}

#define X(tag, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
AST_NARYNODE_TABLE
#undef X

bool EvalNode::validateNode(NodeVectorType& Parents) {
  const auto* Sym = dyn_cast<SymbolNode>(getKid(0));
  assert(Sym);
  const auto* Defn = dyn_cast<DefineNode>(Sym->getDefineDefinition());
  if (Defn == nullptr) {
    fprintf(stderr, "Can't find define for symbol!\n");
    describeNode("In", this);
    return false;
  }
  const auto* Params = dyn_cast<ParamsNode>(Defn->getKid(1));
  assert(Params);
  if (int(Params->getValue()) != getNumKids() - 1) {
    fprintf(stderr, "Eval called with wrong number of arguments!\n");
    describeNode("bad eval", this);
    describeNode("called define", Defn);
    return false;
  }
  return true;
}

const CaseNode* SelectBaseNode::getCase(IntType Key) const {
  if (LookupMap.count(Key))
    return LookupMap.at(Key);
  return nullptr;
}

void SelectBaseNode::clearCaches(NodeVectorType& AdditionalNodes) {
  LookupMap.clear();
}

void SelectBaseNode::installCaches(NodeVectorType& AdditionalNodes) {
  for (auto* Kid : *this) {
    if (const auto* Case = dyn_cast<CaseNode>(Kid)) {
      const auto* CaseExp = Case->getKid(0);
      if (const auto* LitUse = dyn_cast<LiteralUseNode>(CaseExp)) {
        SymbolNode* Sym = dyn_cast<SymbolNode>(LitUse->getKid(0));
        if (const auto* LitDef = Sym->getLiteralDefinition()) {
          CaseExp = LitDef->getKid(1);
        }
      }
      if (const auto* Key = dyn_cast<IntegerNode>(CaseExp)) {
        LookupMap[Key->getValue()] = Case;
      }
    }
  }
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

namespace {

constexpr uint32_t MaxOpcodeWidth = 64;

IntType getWidthMask(uint32_t BitWidth) {
  return std::numeric_limits<IntType>::max() >> (MaxOpcodeWidth - BitWidth);
}

IntType getIntegerValue(Node* N) {
  if (auto* IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

bool getCaseSelectorWidth(const Node* Nd, uint32_t& Width) {
  switch (Nd->getType()) {
    default:
      // Not allowed in opcode cases.
      describeNode("Non-fixed width opcode format", Nd);
      return false;
    case OpUint8:
    case OpUint32:
    case OpUint64:
      break;
  }
  Width = getIntegerValue(Nd->getKid(0));
  if (Width == 0 || Width >= MaxOpcodeWidth) {
    describeNode("Bit size not valid", Nd);
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
      describeNode("Non-fixed width opcode format", Nd);
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
            describeNode("Inside", Nd);
            return false;
          }
        }
      } else {
        uint32_t Width;
        if (!getCaseSelectorWidth(Nd->getKid(0), Width)) {
          describeNode("Inside", Nd);
          return false;
        }
        if (Width >= MaxOpcodeWidth) {
          describeNode("Bit width(s) too big", Nd);
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
            describeNode("Inside", Nd);
            return false;
          }
          for (uint32_t CaseWidth : LocalCaseWidths) {
            uint32_t CombinedWidth = Width + CaseWidth;
            if (CombinedWidth >= MaxOpcodeWidth) {
              describeNode("Bit width(s) too big", Nd);
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
  void tag##Node::forceCompilation() {}
AST_SELECTNODE_TABLE
#undef X

void OpcodeNode::clearCaches(NodeVectorType& AdditionalNodes) {
  SelectBaseNode::clearCaches(AdditionalNodes);
  CaseRangeVector.clear();
}

void OpcodeNode::installCaseRanges() {
  uint32_t InitialWidth;
  if (!getCaseSelectorWidth(getKid(0), InitialWidth)) {
    describeNode("Inside", this);
    fatal("Unable to install caches for opcode s-expression");
  }
  for (int i = 1, NumKids = getNumKids(); i < NumKids; ++i) {
    assert(isa<CaseNode>(Kids[i]));
    const CaseNode* Case = cast<CaseNode>(Kids[i]);
    std::unordered_set<uint32_t> CaseWidths;
    IntType Key = getIntegerValue(Case->getKid(0));
    if (!collectCaseWidths(Key, Case->getKid(1), CaseWidths)) {
      describeNode("Inside", Case);
      describeNode("Inside ", this);
      fatal("Unable to install caches for opcode s-expression");
    }
    for (uint32_t NestedWidth : CaseWidths) {
      uint32_t Width = InitialWidth + NestedWidth;
      if (Width > MaxOpcodeWidth) {
        describeNode("Bit width(s) too big", this);
        fatal("Unable to install caches for opcode s-expression");
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
      describeNode("Range 1", R1.getCase());
      describeNode("Range 2", R2.getCase());
      describeNode("Inside", this);
      fatal("Opcode case ranges not unique");
    }
  }
}

void OpcodeNode::installCaches(NodeVectorType& AdditionalNodes) {
  TRACE_METHOD("OpcodeNode::installCaches");
  TRACE(node_ptr, nullptr, this);
  SelectBaseNode::installCaches(AdditionalNodes);
  installCaseRanges();
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

}  // end of namespace filt

}  // end of namespace wasm
