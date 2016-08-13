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
#include "sexp/TextWriter.h"
#include "utils/Defs.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace utils;

namespace filt {

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

// Note: we create duumy virtual forceCompilation() to force legal
// class definitions to be compiled in here. Classes not explicitly
// instantiated here will not link. This used to force an error if
// a node type is not defined with the correct template class.

void SymbolNode::clearCaches(NodeVectorType& AdditionalNodes) {
  if (DefineDefinition)
    AdditionalNodes.push_back(DefineDefinition);
}

void SymbolNode::installCaches(NodeVectorType& AdditionalNodes) {
  if (DefineDefinition)
    AdditionalNodes.push_back(DefineDefinition);
}

std::string SymbolNode::getStringName() const {
  std::string Str(Name.size(), '\0');
  for (size_t i = 0; i < Name.size(); ++i)
    Str[i] = char(Name[i]);
  return Str;
}

SymbolTable::SymbolTable()
    // TODO(karlschimpf) Switch Alloc to an ArenaAllocator once working.
    : Trace("sexp st"),
      Alloc(std::make_shared<Malloc>()),
      NextCreationIndex(0) {
  Error = Alloc->create<ErrorNode>(*this);
}

SymbolNode* SymbolTable::getSymbolDefinition(ExternalName& Name) {
  SymbolNode* Node = SymbolMap[Name];
  if (Node == nullptr) {
    Node = create<SymbolNode>(Name);
    SymbolMap[Name] = Node;
  }
  return Node;
}

void SymbolTable::install(Node* Root) {
  TRACE_METHOD("install", Trace);
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
  TRACE_METHOD("clearSubtreeCaches", Trace);
  TRACE_SEXP(nullptr, Nd);
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
  TRACE_METHOD("installSubtreeCaches", Trace);
  TRACE_SEXP(nullptr, Nd);
  VisitedNodes.insert(Nd);
  Nd->installCaches(AdditionalNodes);
  for (auto* Kid : *Nd)
    AdditionalNodes.push_back(Kid);
}

void SymbolTable::installDefinitions(Node* Root) {
  TRACE_METHOD("installDefinitions", Trace);
  TRACE_SEXP(nullptr, Root);
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
      Trace.printSexp("Malformed", Root);
      fatal("Malformed define s-expression found!");
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
      Trace.printSexp("Malformed", Root);
      fatal("Malformed rename s-expression found!");
      return;
    }
    case OpUndefine: {
      if (auto* UndefineSymbol = dyn_cast<SymbolNode>(Root->getKid(0))) {
        UndefineSymbol->setDefineDefinition(nullptr);
        return;
      }
      Trace.printSexp("Can't undefine", Root);
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
#define X(tag, NODE_DECLS) \
  case Op##tag:            \
    return true;
      AST_INTEGERNODE_TABLE
#undef X
  }
}

#define X(tag, NODE_DECLS) \
  void tag##Node::forceCompilation() {}
AST_INTEGERNODE_TABLE
#undef X

bool ParamNode::validateNode(NodeVectorType& Parents) {
  TRACE_METHOD("validateNode", getTrace());
  TRACE_SEXP(nullptr, this);
  for (size_t i = Parents.size(); i > 0; --i) {
    auto* Nd = Parents[i - 1];
    auto* Define = cast<DefineNode>(Nd);
    if (Define == nullptr)
      continue;
    TRACE_SEXP("Enclosing define", Nd);
    // Don't complain about this if specifying number of parameters for define.
    if (i == Parents.size() && this == Define->getKid(1))
      return true;
    // Scope found. Check if parameter is legal.
    if (!Define->isValidParam(getValue())) {
      fprintf(getTrace().getFile(),
              "Error: Param %" PRIuMAX " not defined for method: %s\n",
              uintmax_t(getValue()), Define->getName().c_str());
      return false;
    }
    auto* Sym = dyn_cast<SymbolNode>(Define->getKid(0));
    if (Sym == nullptr)
      return false;
    DefiningSymbol = Sym;
    TRACE_SEXP("DefiningSymbol", DefiningSymbol.get());
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
  assert(isa<ParamNode>(getKid(1)));
  return Index < cast<ParamNode>(getKid(1))->getValue();
}

std::string DefineNode::getName() const {
  assert(isa<SymbolNode>(getKid(0)));
  return dyn_cast<SymbolNode>(getKid(0))->getStringName();
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
  void tag##Node::forceCompilation() {}
AST_NARYNODE_TABLE
#undef X

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
      if (const auto* Key = dyn_cast<IntegerNode>(Case->getKid(0))) {
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

void OpcodeNode::WriteRange::traceInternal(const char* Prefix,
                                           TraceClassSexp& Trace) const {
  FILE* Out = Trace.getFile();
  Trace.indent();
  fprintf(Out, "[%" PRIxMAX "..%" PRIxMAX "](%" PRIuMAX "):\n", uintmax_t(Min),
          uintmax_t(Max), uintmax_t(ShiftValue));
  TRACE_SEXP_USING(Trace, nullptr, Case);
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
      Nd->getTrace().printSexp("Non-fixed width opcode format", Nd);
      return false;
    case OpUint8NoArgs:
      Width = 8;
      return true;
    case OpUint8OneArg:
      break;
    case OpUint32NoArgs:
      Width = 32;
      return true;
    case OpUint32OneArg:
      break;
    case OpUint64NoArgs:
      Width = 64;
      return true;
    case OpUint64OneArg:
      break;
  }
  Width = getIntegerValue(Nd->getKid(0));
  if (Width == 0 || Width >= MaxOpcodeWidth) {
    Nd->getTrace().printSexp("Bit size not valid", Nd);
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
      Nd->getTrace().printSexp("Non-fixed width opcode format", Nd);
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
            Nd->getTrace().printSexp("Inside", Nd);
            return false;
          }
        }
      } else {
        uint32_t Width;
        if (!getCaseSelectorWidth(Nd->getKid(0), Width)) {
          Nd->getTrace().printSexp("Inside", Nd);
          return false;
        }
        if (Width >= MaxOpcodeWidth) {
          Nd->getTrace().printSexp("Bit width(s) too big", Nd);
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
            Nd->getTrace().printSexp("Inside", Nd);
            return false;
          }
          for (uint32_t CaseWidth : LocalCaseWidths) {
            uint32_t CombinedWidth = Width + CaseWidth;
            if (CombinedWidth >= MaxOpcodeWidth) {
              Nd->getTrace().printSexp("Bit width(s) too big", Nd);
              return false;
            }
            CaseWidths.insert(CombinedWidth);
          }
        }
      }
      return true;
    case OpUint8NoArgs:
    case OpUint8OneArg:
    case OpUint32NoArgs:
    case OpUint32OneArg:
    case OpUint64NoArgs:
    case OpUint64OneArg:
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
    getTrace().printSexp("Inside", this);
    fatal("Unable to install caches for opcode s-expression");
  }
  for (int i = 1, NumKids = getNumKids(); i < NumKids; ++i) {
    assert(isa<CaseNode>(Kids[i]));
    const CaseNode* Case = cast<CaseNode>(Kids[i]);
    std::unordered_set<uint32_t> CaseWidths;
    IntType Key = getIntegerValue(Case->getKid(0));
    if (!collectCaseWidths(Key, Case->getKid(1), CaseWidths)) {
      getTrace().printSexp("Inside", Case);
      getTrace().printSexp("Inside ", this);
      fatal("Unable to install caches for opcode s-expression");
    }
    for (uint32_t NestedWidth : CaseWidths) {
      uint32_t Width = InitialWidth + NestedWidth;
      if (Width > MaxOpcodeWidth) {
        getTrace().printSexp("Bit width(s) too big", this);
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
      getTrace().printSexp("Range 1", R1.getCase());
      getTrace().printSexp("Range 2", R2.getCase());
      getTrace().printSexp("Inside", this);
      fatal("Opcode case ranges not unique");
    }
  }
}

void OpcodeNode::installCaches(NodeVectorType& AdditionalNodes) {
  TRACE_METHOD("OpcodeNode::installCaches", getTrace());
  TRACE_SEXP(nullptr, this);
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
