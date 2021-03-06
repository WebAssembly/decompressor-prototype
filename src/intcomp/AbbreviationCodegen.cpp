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
#include "algorithms/cism0x0.h"

#if 1
#include "sexp/TextWriter.h"
#endif

#include <algorithm>

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace intcomp {

namespace {

//#define X(NAME, VALUE)
#define SPECIAL_ABBREV_TABLE         \
  X(CismDefaultSingleValue, 16767)   \
  X(CismDefaultMultipleValue, 16766) \
  X(CismBlockEnterValue, 16768)      \
  X(CismBlockExitValue, 16769)       \
  X(CismAlignValue, 16770)

enum class SpecialAbbrev : IntType {
#define X(NAME, VALUE) NAME = VALUE,
  SPECIAL_ABBREV_TABLE
#undef X
      Unknown = ~(IntType(0))
};

static IntType SpecialAbbrevs[] = {
#define X(NAME, VALUE) IntType(SpecialAbbrev::NAME),
    SPECIAL_ABBREV_TABLE
#undef X
};

}  // end of anonymous namespace

AbbreviationCodegen::AbbreviationCodegen(const CompressionFlags& Flags,
                                         CountNode::RootPtr Root,
                                         HuffmanEncoder::NodePtr EncodingRoot,
                                         CountNode::PtrSet& Assignments,
                                         bool ToRead)
    : Flags(Flags),
      Root(Root),
      EncodingRoot(EncodingRoot),
      Assignments(Assignments),
      ToRead(ToRead),
      CategorizeName("categorize"),
      OpcodeName("opcode"),
      ProcessName("process"),
      ValuesName("values"),
      OldName(".old") {}

AbbreviationCodegen::~AbbreviationCodegen() {}

Node* AbbreviationCodegen::generateHeader(NodeType Type,
                                          uint32_t MagicNumber,
                                          uint32_t VersionNumber) {
  Header* Header = nullptr;
  switch (Type) {
    default:
      return Symtab->create<Void>();
    case NodeType::SourceHeader:
      Header = Symtab->create<SourceHeader>();
      break;
    case NodeType::ReadHeader:
      Header = Symtab->create<ReadHeader>();
      break;
    case NodeType::WriteHeader:
      Header = Symtab->create<WriteHeader>();
      break;
  }
  Header->append(
      Symtab->create<U32Const>(MagicNumber, ValueFormat::Hexidecimal));
  Header->append(
      Symtab->create<U32Const>(VersionNumber, ValueFormat::Hexidecimal));
  return Header;
}

void AbbreviationCodegen::generateFunctions(Algorithm* Alg) {
  if (!Flags.UseCismModel)
    return Alg->append(generateStartFunction());

  Alg->append(generateEnclosingAlg("cism"));
  if (!ToRead) {
    Alg->append(generateRename(ProcessName));
    Alg->append(generateProcessFunction());
    Alg->append(generateValuesFunction());
  }
  Alg->append(generateOpcodeFunction());
  Alg->append(generateCategorizeFunction());
}

Node* AbbreviationCodegen::generateValuesFunction() {
  auto* Fcn = Symtab->create<Define>();
  Fcn->append(Symtab->getOrCreateSymbol(ValuesName));
  Fcn->append(Symtab->create<NoParams>());
  Fcn->append(Symtab->create<NoLocals>());
  Fcn->append(Symtab->create<Loop>(Symtab->create<Varuint64>(),
                                   Symtab->create<Varint64>()));
  return Fcn;
}

Node* AbbreviationCodegen::generateProcessFunction() {
  auto* Fcn = Symtab->create<Define>();
  Fcn->append(Symtab->getOrCreateSymbol(ProcessName));
  Fcn->append(Symtab->create<ParamValues>(1, ValueFormat::Decimal));
  Fcn->append(Symtab->create<NoLocals>());
  auto* Swch = Symtab->create<Switch>();
  Fcn->append(Swch);
  Swch->append(Symtab->create<Param>(0, ValueFormat::Decimal));
  auto* Eval = Symtab->create<EvalVirtual>();
  Swch->append(Eval);
  Eval->append(generateOld(ProcessName));
  Eval->append(Symtab->create<Param>(0, ValueFormat::Decimal));
  Swch->append(Symtab->create<Case>(
      Symtab->create<U64Const>(IntType(SpecialAbbrev::CismBlockEnterValue),
                               ValueFormat::Decimal),
      generateCallback(PredefinedSymbol::Block_enter_writeonly)));
  Swch->append(Symtab->create<Case>(
      Symtab->create<U64Const>(IntType(SpecialAbbrev::CismBlockExitValue),
                               ValueFormat::Decimal),
      generateCallback(PredefinedSymbol::Block_exit_writeonly)));
  return Fcn;
}

Node* AbbreviationCodegen::generateOpcodeFunction() {
  auto* Fcn = Symtab->create<Define>();
  Fcn->append(Symtab->getOrCreateSymbol(OpcodeName));
  Fcn->append(Symtab->create<NoParams>());
  if (Flags.AlignOpcodes)
    Fcn->append(Symtab->create<Locals>(1, ValueFormat::Decimal));
  else
    Fcn->append(Symtab->create<NoLocals>());
  Node* Rd = generateAbbreviationRead();
  if (!Flags.AlignOpcodes) {
    Fcn->append(Rd);
    return Fcn;
  }
  Sequence* Seq = Symtab->create<Sequence>();
  Fcn->append(Seq);
  Seq->append(
      Symtab->create<Set>(Symtab->create<Local>(0, ValueFormat::Decimal), Rd));
  Seq->append(generateCallback(PredefinedSymbol::Align));
  Seq->append(Symtab->create<Local>(0, ValueFormat::Decimal));
  return Fcn;
}

Node* AbbreviationCodegen::generateCategorizeFunction() {
  auto* Fcn = Symtab->create<Define>();
  Fcn->append(Symtab->getOrCreateSymbol(CategorizeName));
  Fcn->append(Symtab->create<ParamValues>(1, ValueFormat::Decimal));
  Fcn->append(Symtab->create<NoLocals>());
  auto* MapNd = Symtab->create<Map>();
  Fcn->append(MapNd);
  MapNd->append(Symtab->create<Param>(0, ValueFormat::Decimal));

  // Define remappings for each special action.
  std::map<IntType, IntType> FixMap;
  {
    // Start by collecting set of used abbreviations.
    std::unordered_set<IntType> Used;
    for (CountNode::Ptr Nd : Assignments) {
      assert(Nd->hasAbbrevIndex());
      Used.insert(Nd->getAbbrevIndex());
    }
    // Now find available renames for conflicting values.
    IntType NextAvail = 0;
    for (size_t i = 0; i < size(SpecialAbbrevs); ++i) {
      IntType Val = SpecialAbbrevs[i];
      if (Used.count(Val)) {
        while (Used.count(NextAvail))
          ++NextAvail;
      }
      FixMap[Val] = NextAvail++;
    }
  }

  std::map<IntType, IntType> CatMap;
  for (CountNode::Ptr Nd : Assignments) {
    assert(Nd->hasAbbrevIndex());
    IntType Val = Nd->getAbbrevIndex();
    if (FixMap.count(Val)) {
      CatMap[Val] = FixMap[Val];
      continue;
    }
    switch (Nd->getKind()) {
      default:
        break;
      case CountNode::Kind::Default:
        CatMap[Nd->getAbbrevIndex()] =
            (IntType(cast<DefaultCountNode>(Nd.get())->isSingle()
                         ? SpecialAbbrev::CismDefaultSingleValue
                         : SpecialAbbrev::CismDefaultMultipleValue));
        break;
      case CountNode::Kind::Block:
        CatMap[Nd->getAbbrevIndex()] =
            IntType(cast<BlockCountNode>(Nd.get())->isEnter()
                        ? SpecialAbbrev::CismBlockEnterValue
                        : SpecialAbbrev::CismBlockExitValue);
        break;
      case CountNode::Kind::Align:
        CatMap[Nd->getAbbrevIndex()] = IntType(SpecialAbbrev::CismAlignValue);
        break;
    }
  }
  for (const auto& Pair : CatMap)
    MapNd->append(generateMapCase(Pair.first, Pair.second));
  return Fcn;
}

Node* AbbreviationCodegen::generateMapCase(IntType Index, IntType Value) {
  return Symtab->create<Case>(
      Symtab->create<U64Const>(Index, ValueFormat::Decimal),
      Symtab->create<U64Const>(Value, ValueFormat::Decimal));
}

Node* AbbreviationCodegen::generateEnclosingAlg(charstring Name) {
  auto* Enc = Symtab->create<EnclosingAlgorithms>();
  Enc->append(Symtab->getOrCreateSymbol(Name));
  return Enc;
}

Node* AbbreviationCodegen::generateOld(std::string Name) {
  return Symtab->getOrCreateSymbol(Name + OldName);
}

Node* AbbreviationCodegen::generateRename(std::string Name) {
  Node* From = Symtab->getOrCreateSymbol(Name);
  Node* To = generateOld(Name);
  return Symtab->create<Rename>(From, To);
}

Node* AbbreviationCodegen::generateStartFunction() {
  auto* Fcn = Symtab->create<Define>();
  Fcn->append(Symtab->getPredefined(PredefinedSymbol::File));
  Fcn->append(Symtab->create<NoParams>());
  Fcn->append(Symtab->create<NoLocals>());
  Fcn->append(Symtab->create<LoopUnbounded>(generateSwitchStatement()));
  return Fcn;
}

Node* AbbreviationCodegen::generateAbbreviationRead() {
  auto* Format =
      EncodingRoot
          ? Symtab->create<BinaryEval>(generateHuffmanEncoding(EncodingRoot))
          : generateAbbrevFormat(Flags.AbbrevFormat);
  if (ToRead) {
    Format = Symtab->create<Read>(Format);
  }
  return Format;
}

Node* AbbreviationCodegen::generateHuffmanEncoding(
    HuffmanEncoder::NodePtr Root) {
  Node* Result = nullptr;
  switch (Root->getType()) {
    case HuffmanEncoder::NodeType::Selector: {
      HuffmanEncoder::Selector* Sel =
          cast<HuffmanEncoder::Selector>(Root.get());
      Result =
          Symtab->create<BinarySelect>(generateHuffmanEncoding(Sel->getKid1()),
                                       generateHuffmanEncoding(Sel->getKid2()));
      break;
    }
    case HuffmanEncoder::NodeType::Symbol:
      Result = Symtab->create<BinaryAccept>();
      break;
  }
  return Result;
}

Node* AbbreviationCodegen::generateSwitchStatement() {
  auto* SwitchStmt = Symtab->create<Switch>();
  SwitchStmt->append(generateAbbreviationRead());
  SwitchStmt->append(Symtab->create<Error>());
  // TODO(karlschimpf): Sort so that output consistent or more readable?
  for (CountNode::Ptr Nd : Assignments) {
    assert(Nd->hasAbbrevIndex());
    SwitchStmt->append(generateCase(Nd->getAbbrevIndex(), Nd));
  }
  return SwitchStmt;
}

Node* AbbreviationCodegen::generateCase(size_t AbbrevIndex, CountNode::Ptr Nd) {
  return Symtab->create<Case>(
      Symtab->create<U64Const>(AbbrevIndex, decode::ValueFormat::Decimal),
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
  else if (isa<AlignCountNode>(NdPtr))
    return generateCallback(PredefinedSymbol::Align);
  return Symtab->create<Error>();
}

Node* AbbreviationCodegen::generateCallback(PredefinedSymbol Sym) {
  return Symtab->create<Callback>(
      Symtab->create<LiteralActionUse>(Symtab->getPredefined(Sym)));
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
  return generateCallback(Sym);
}

Node* AbbreviationCodegen::generateDefaultAction(DefaultCountNode* Default) {
  return Default->isSingle() ? generateDefaultSingleAction()
                             : generateDefaultMultipleAction();
}

Node* AbbreviationCodegen::generateDefaultMultipleAction() {
  Node* LoopSize = Symtab->create<Varuint64>();
  if (ToRead)
    LoopSize = Symtab->create<Read>(LoopSize);
  return Symtab->create<Loop>(LoopSize, generateDefaultSingleAction());
}

Node* AbbreviationCodegen::generateDefaultSingleAction() {
  return Symtab->create<Varint64>();
}

Node* AbbreviationCodegen::generateIntType(IntType Value) {
  return Symtab->create<U64Const>(Value, decode::ValueFormat::Decimal);
}

Node* AbbreviationCodegen::generateIntLitAction(IntCountNode* Nd) {
  return ToRead ? generateIntLitActionRead(Nd) : generateIntLitActionWrite(Nd);
}

Node* AbbreviationCodegen::generateIntLitActionRead(IntCountNode* Nd) {
  std::vector<IntCountNode*> Values;
  while (Nd) {
    Values.push_back(Nd);
    Nd = Nd->getParent().get();
  }
  std::reverse(Values.begin(), Values.end());
  auto* W = Symtab->create<Write>();
  W->append(Symtab->create<Varuint64>());
  for (IntCountNode* Nd : Values)
    W->append(generateIntType(Nd->getValue()));
  return W;
}

Node* AbbreviationCodegen::generateIntLitActionWrite(IntCountNode* Nd) {
  return Symtab->create<Void>();
}

std::shared_ptr<SymbolTable> AbbreviationCodegen::getCodeSymtab() {
  Symtab = std::make_shared<SymbolTable>();
  auto* Alg = Symtab->create<Algorithm>();
  Alg->append(generateHeader(NodeType::SourceHeader, CasmBinaryMagic,
                             CasmBinaryVersion));
  if (Flags.UseCismModel) {
    Symtab->setEnclosingScope(getAlgcism0x0Symtab());
    if (ToRead) {
      Alg->append(generateHeader(NodeType::ReadHeader, CismBinaryMagic,
                                 CismBinaryVersion));
      Alg->append(generateHeader(NodeType::WriteHeader, WasmBinaryMagic,
                                 WasmBinaryVersionD));
    } else {
      Alg->append(generateHeader(NodeType::ReadHeader, WasmBinaryMagic,
                                 WasmBinaryVersionD));
      Alg->append(generateHeader(NodeType::WriteHeader, CismBinaryMagic,
                                 CismBinaryVersion));
    }
  } else {
    Alg->append(generateHeader(NodeType::ReadHeader, WasmBinaryMagic,
                               WasmBinaryVersionD));
  }
  generateFunctions(Alg);
  Symtab->setAlgorithm(Alg);
  Symtab->install();
  return Symtab;
}

Node* AbbreviationCodegen::generateAbbrevFormat(IntTypeFormat AbbrevFormat) {
  switch (AbbrevFormat) {
    case IntTypeFormat::Uint8:
      return Symtab->create<Uint8>();
    case IntTypeFormat::Varint32:
      return Symtab->create<Varint32>();
    case IntTypeFormat::Varuint32:
      return Symtab->create<Varuint32>();
    case IntTypeFormat::Uint32:
      return Symtab->create<Uint32>();
    case IntTypeFormat::Varint64:
      return Symtab->create<Varint64>();
    case IntTypeFormat::Varuint64:
      return Symtab->create<Varuint64>();
    case IntTypeFormat::Uint64:
      return Symtab->create<Uint64>();
  }
  WASM_RETURN_UNREACHABLE(nullptr);
}

}  // end of namespace intcomp

}  // end of namespace wasm
