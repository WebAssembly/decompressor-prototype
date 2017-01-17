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

// Implements an s-exprssion code generator for abbreviations.

#include "intcomp/AbbreviationCodegen.h"

#include <algorithm>

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;

namespace intcomp {

AbbreviationCodegen::AbbreviationCodegen(CountNode::RootPtr Root,
                                         IntTypeFormat AbbrevFormat,
                                         CountNode::PtrVector& Assignments)
    : Root(Root), AbbrevFormat(AbbrevFormat), Assignments(Assignments) {
}

Node* AbbreviationCodegen::generateCasmFileHeader() {
  auto* Header = Symtab->create<FileHeaderNode>();
  Header->append(Symtab->getU32ConstDefinition(
      CasmBinaryMagic, decode::ValueFormat::Hexidecimal));
  Header->append(Symtab->getU32ConstDefinition(
      CasmBinaryVersion, decode::ValueFormat::Hexidecimal));
  return Header;
}

Node* AbbreviationCodegen::generateWasmFileHeader() {
  auto* Header = Symtab->create<FileHeaderNode>();
  Header->append(Symtab->getU32ConstDefinition(
      WasmBinaryMagic, decode::ValueFormat::Hexidecimal));
  Header->append(Symtab->getU32ConstDefinition(
      WasmBinaryVersionD, decode::ValueFormat::Hexidecimal));
  return Header;
}

void AbbreviationCodegen::generateFile(Node* SourceHeader, Node* TargetHeader) {
  auto* File =
      Symtab->create<FileNode>(SourceHeader, TargetHeader, generateFileBody());
  Symtab->install(File);
}

AbbreviationCodegen::~AbbreviationCodegen() {
}

Node* AbbreviationCodegen::generateFileBody() {
  auto* Body = Symtab->create<SectionNode>();
  Body->append(generateFileFcn());
  return Body;
}

Node* AbbreviationCodegen::generateFileFcn() {
  auto* Fcn = Symtab->create<DefineNode>();
  Fcn->append(Symtab->getPredefined(PredefinedSymbol::File));
  Fcn->append(Symtab->getParamsDefinition());
  Fcn->append(Symtab->create<LoopUnboundedNode>(generateSwitchStatement()));
  return Fcn;
}

Node* AbbreviationCodegen::generateAbbreviationRead() {
  auto* Format = generateAbbrevFormat(AbbrevFormat);
  if (ToRead) {
    Format = Symtab->create<ReadNode>(Format);
  }
  return Format;
}

Node* AbbreviationCodegen::generateSwitchStatement() {
  auto* SwitchStmt = Symtab->create<SwitchNode>();
  SwitchStmt->append(generateAbbreviationRead());
  SwitchStmt->append(Symtab->create<ErrorNode>());
  for (size_t i = 0; i < Assignments.size(); ++i)
    SwitchStmt->append(generateCase(i, Assignments[i]));
  return SwitchStmt;
}

Node* AbbreviationCodegen::generateCase(size_t AbbrevIndex, CountNode::Ptr Nd) {
  return Symtab->create<CaseNode>(
      Symtab->getU64ConstDefinition(AbbrevIndex, decode::ValueFormat::Decimal),
      generateAction(Nd));
}

Node* AbbreviationCodegen::generateAction(CountNode::Ptr Nd) {
  CountNode* NdPtr = Nd.get();
  if (auto* CntNd = dyn_cast<IntCountNode>(NdPtr))
    return generateIntLitAction(CntNd);
  else if (auto* BlkPtr = dyn_cast<BlockCountNode>(NdPtr))
    return generateBlockAction(BlkPtr);
  else if (auto* DefaultPtr = dyn_cast<DefaultCountNode>(NdPtr))
    return generateDefaultAction(DefaultPtr);
  return Symtab->create<ErrorNode>();
}

Node* AbbreviationCodegen::generateBlockAction(BlockCountNode* Blk) {
  PredefinedSymbol Sym;
  if (Blk->isEnter()) {
    Sym = ToRead ? PredefinedSymbol::Block_enter
                 : PredefinedSymbol::Block_enter_writeonly;
  } else {
    Sym = ToRead ? PredefinedSymbol::Block_exit
                 : PredefinedSymbol::Block_exit_writeonly;
  }
  return Symtab->create<CallbackNode>(Symtab->getPredefined(Sym));
}

Node* AbbreviationCodegen::generateDefaultAction(DefaultCountNode* Default) {
  return Default->isSingle() ? generateDefaultSingleAction()
                             : generateDefaultMultipleAction();
}

Node* AbbreviationCodegen::generateDefaultMultipleAction() {
  Node* LoopSize = Symtab->getVaruint64Definition();
  if (ToRead)
    LoopSize = Symtab->create<ReadNode>(LoopSize);
  return Symtab->create<LoopNode>(LoopSize, generateDefaultSingleAction());
}

Node* AbbreviationCodegen::generateDefaultSingleAction() {
  return Symtab->getVarint64Definition();
}

Node* AbbreviationCodegen::generateIntType(IntType Value) {
  return Symtab->getU64ConstDefinition(Value, decode::ValueFormat::Decimal);
}

Node* AbbreviationCodegen::generateIntLitAction(IntCountNode* Nd) {
  return ToRead ? generateIntLitActionRead(Nd) : generateIntLitActionWrite(Nd);
}

Node* AbbreviationCodegen::generateIntLitActionRead(IntCountNode* Nd) {
  if (Nd->getPathLength() == 1)
    return generateIntType(Nd->getValue());
  std::vector<IntCountNode*> Values;
  while (Nd) {
    Values.push_back(Nd);
    Nd = Nd->getParent().get();
  }
  std::reverse(Values.begin(), Values.end());
  auto* Write = Symtab->create<WriteNode>();
  Write->append(Symtab->getVaruint64Definition());
  for (IntCountNode* Nd : Values)
    Write->append(generateIntType(Nd->getValue()));
  return Write;
}

Node* AbbreviationCodegen::generateIntLitActionWrite(IntCountNode* Nd) {
  return Symtab->create<VoidNode>();
}

std::shared_ptr<SymbolTable> AbbreviationCodegen::getCodeSymtab(bool ToRead) {
  this->ToRead = ToRead;
  Symtab = std::make_shared<SymbolTable>();
  generateFile(generateCasmFileHeader(), generateWasmFileHeader());
  return Symtab;
}

Node* AbbreviationCodegen::generateAbbrevFormat(IntTypeFormat AbbrevFormat) {
  switch (AbbrevFormat) {
    case IntTypeFormat::Uint8:
      return Symtab->getUint8Definition();
    case IntTypeFormat::Varint32:
      return Symtab->getVarint32Definition();
    case IntTypeFormat::Varuint32:
      return Symtab->getVaruint32Definition();
    case IntTypeFormat::Uint32:
      return Symtab->getUint32Definition();
    case IntTypeFormat::Varint64:
      return Symtab->getVarint64Definition();
    case IntTypeFormat::Varuint64:
      return Symtab->getVaruint64Definition();
    case IntTypeFormat::Uint64:
      return Symtab->getUint64Definition();
  }
  WASM_RETURN_UNREACHABLE(nullptr);
}

}  // end of namespace intcomp

}  // end of namespace wasm
