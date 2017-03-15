// -*- C++ -*-
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

// IMplements a writer to convert a CASM stream into the corresponding
// AST algorithm.

#include "casm/InflateAst.h"

#include "binary/SectionSymbolTable.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "utils/Casting.h"
#include "utils/Trace.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

InflateAst::InflateAst()
    : Writer(false),
      Symtab(std::make_shared<SymbolTable>()),
      SectionSymtab(utils::make_unique<SectionSymbolTable>(Symtab)),
      ValuesTop(0),
      Values(ValuesTop),
      AstsTop(nullptr),
      Asts(AstsTop),
      SymbolNameSize(0),
      ValueMarker(0),
      AstMarkersTop(0),
      AstMarkers(AstMarkersTop),
      InstallDuringInflation(true) {
}

InflateAst::~InflateAst() {
}

bool InflateAst::failBuild(const char* Method, std::string Message) {
  TRACE_METHOD(Method);
  TRACE_MESSAGE(Message);
  TRACE_MESSAGE("Can't continue");
  return false;
}

bool InflateAst::failWriteActionMalformed() {
  return failBuild("writeAction", "Input malformed");
}

bool InflateAst::failWriteHeaderMalformed() {
  return failBuild("writeHeader", "Input malformed");
}

FileNode* InflateAst::getGeneratedFile() const {
  if (Asts.size() != 1)
    return nullptr;
  return dyn_cast<FileNode>(AstsTop);
}

template <class T>
bool InflateAst::buildNullary() {
  Values.pop();
  Asts.push(Symtab->create<T>());
  TRACE(node_ptr, "Tree", AstsTop);
  return true;
}

template <class T>
bool InflateAst::buildUnary() {
  Values.pop();
  Asts.push(Symtab->create<T>(Asts.popValue()));
  TRACE(node_ptr, "Tree", AstsTop);
  return true;
}

template <class T>
bool InflateAst::buildBinary() {
  Values.pop();
  Node* Arg2 = Asts.popValue();
  Node* Arg1 = Asts.popValue();
  Asts.push(Symtab->create<T>(Arg1, Arg2));
  TRACE(node_ptr, "Tree", AstsTop);
  return true;
}

template <class T>
bool InflateAst::buildTernary() {
  Values.pop();
  Node* Arg3 = Asts.popValue();
  Node* Arg2 = Asts.popValue();
  Node* Arg1 = Asts.popValue();
  Asts.push(Symtab->create<T>(Arg1, Arg2, Arg3));
  TRACE(node_ptr, "Tree", AstsTop);
  return true;
}

template <class T>
bool InflateAst::buildNary() {
  return appendArgs(Symtab->create<T>());
}

bool InflateAst::appendArgs(Node* Nd) {
  size_t NumArgs = Values.popValue();
  Values.pop();
  for (size_t i = Asts.size() - NumArgs; i < Asts.size(); ++i)
    Nd->append(Asts[i]);
  for (size_t i = 0; i < NumArgs; ++i)
    Asts.pop();
  TRACE(node_ptr, "Tree", Nd);
  Asts.push(Nd);
  return true;
}

// TODO(karlschimpf) Should we extend StreamType to have other?
StreamType InflateAst::getStreamType() const {
  return StreamType::Other;
}

const char* InflateAst::getDefaultTraceName() const {
  return "InflateAst";
}

bool InflateAst::writeUint8(uint8_t Value) {
  return write(Value);
}

bool InflateAst::writeUint32(uint32_t Value) {
  return write(Value);
}

bool InflateAst::writeUint64(uint64_t Value) {
  return write(Value);
}

bool InflateAst::writeVarint32(int32_t Value) {
  return write(Value);
}

bool InflateAst::writeVarint64(int64_t Value) {
  return write(Value);
}

bool InflateAst::writeVaruint32(uint32_t Value) {
  return write(Value);
}

bool InflateAst::writeVaruint64(uint64_t Value) {
  return write(Value);
}

bool InflateAst::writeTypedValue(decode::IntType Value, interp::IntTypeFormat) {
  return write(Value);
}

bool InflateAst::writeHeaderValue(decode::IntType Value,
                                  interp::IntTypeFormat Format) {
  if (Asts.empty())
    Asts.push(Symtab->create<FileHeaderNode>());
  if (Asts.size() != 1)
    return failWriteHeaderMalformed();
  Node* Header = AstsTop;
  switch (Format) {
    case IntTypeFormat::Uint8:
      Asts.push(Symtab->getU8ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint32:
      Asts.push(Symtab->getU32ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint64:
      Asts.push(Symtab->getU64ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    default:
      return failWriteHeaderMalformed();
  }
  Header->append(Asts.popValue());
  return true;
}

bool InflateAst::applyOp(IntType Op) {
  switch (NodeType(Op)) {
    case OpAnd:
      return buildBinary<AndNode>();
    case OpBinaryAccept:
      return buildNullary<BinaryAcceptNode>();
    case OpBinaryEval:
      return buildUnary<BinaryEvalNode>();
    case OpBinarySelect:
      return buildBinary<BinarySelectNode>();
    case OpBit:
      return buildNullary<BitNode>();
    case OpBitwiseAnd:
      return buildBinary<BitwiseAndNode>();
    case OpBitwiseOr:
      return buildBinary<BitwiseOrNode>();
    case OpBitwiseNegate:
      return buildUnary<BitwiseNegateNode>();
    case OpBitwiseXor:
      return buildBinary<BitwiseXorNode>();
    case OpBlock:
      return buildUnary<BlockNode>();
    case OpCallback:
      return buildUnary<CallbackNode>();
    case OpCase:
      return buildBinary<CaseNode>();
    case OpDefine:
      return buildNary<DefineNode>();
    case OpError:
      return buildNullary<ErrorNode>();
    case OpEval:
      return buildNary<EvalNode>();
    case OpFile:
      return buildTernary<FileNode>();
    case OpFileHeader:
      return buildNary<FileHeaderNode>();
    case OpIfThen:
      return buildBinary<IfThenNode>();
    case OpIfThenElse:
      return buildTernary<IfThenElseNode>();
    case OpLastRead:
      return buildNullary<LastReadNode>();
    case OpLastSymbolIs:
      return buildUnary<LastSymbolIsNode>();
    case OpLiteralActionDef:
      return buildBinary<LiteralActionDefNode>();
    case OpLiteralActionUse:
      return buildUnary<LiteralActionUseNode>();
    case OpLiteralDef:
      return buildBinary<LiteralDefNode>();
    case OpLiteralUse:
      return buildUnary<LiteralUseNode>();
    case OpLoop:
      return buildBinary<LoopNode>();
    case OpLoopUnbounded:
      return buildUnary<LoopUnboundedNode>();
    case OpMap:
      return buildNary<MapNode>();
    case OpNot:
      return buildUnary<NotNode>();
    case OpOr:
      return buildBinary<OrNode>();
    case OpPeek:
      return buildUnary<PeekNode>();
    case OpRead:
      return buildUnary<ReadNode>();
    case OpRename:
      return buildBinary<RenameNode>();
    case OpSection: {
      // Note: Bottom element is for file.
      TRACE(size_t, "Tree stack size", Asts.size());
      if (Asts.size() < 2)
        return failWriteActionMalformed();
      Values.push(Asts.size() - 2);
      if (!buildNary<SectionNode>())
        return false;
      Values.push(OpFile);
      if (!buildTernary<FileNode>())
        return false;
      FileNode* File = getGeneratedFile();
      if (File == nullptr)
        return failBuild("InflateAst", "Did not generate a file node");
      if (InstallDuringInflation)
        Symtab->install(File);
      return true;
    }
    case OpSequence:
      return buildNary<SequenceNode>();
    case OpSet:
      return buildBinary<SetNode>();
    case OpSwitch:
      return buildNary<SwitchNode>();
    case OpSymbol: {
      SymbolNode* Sym = SectionSymtab->getIndexSymbol(Values.popValue());
      Values.pop();
      if (Sym == nullptr) {
        return failWriteActionMalformed();
      }
      TRACE(node_ptr, "Tree", Sym);
      Asts.push(Sym);
      return true;
    }
    case OpTable:
      return buildBinary<TableNode>();
    case OpUndefine:
      return buildUnary<UndefineNode>();
    case OpUnknownSection:
      return buildUnary<UnknownSectionNode>();
    case OpUint32:
      return buildNullary<Uint32Node>();
    case OpUint64:
      return buildNullary<Uint64Node>();
    case OpUint8:
      return buildNullary<Uint8Node>();
    case OpVarint32:
      return buildNullary<Varint32Node>();
    case OpVarint64:
      return buildNullary<Varint64Node>();
    case OpVaruint32:
      return buildNullary<Varuint32Node>();
    case OpVaruint64:
      return buildNullary<Varuint64Node>();
    case OpVoid:
      return buildNullary<VoidNode>();
    case OpWrite:
      return buildNary<WriteNode>();
    default:
      return failWriteActionMalformed();
  }
  return failWriteActionMalformed();
}

#if 0
bool InflateAst::writeAction(const filt::SymbolNode* Action)
#else
bool InflateAst::writeAction(IntType Action)
#endif
{
#if 0
  TRACE_BLOCK({
      constexpr size_t WindowSize = 10;
      FILE* Out = getTrace().getFile();
      fputs("*** Values ***\n", Out);
      size_t StartIndex = Values.size() > WindowSize
          ? Values.size() - WindowSize : 1;
      if (StartIndex > 1)
        fprintf(Out, "...[%" PRIuMAX "]\n", uintmax_t(Values.size() - (StartIndex - 1)));
     for (IntType Val : Values.iterRange(StartIndex))
        fprintf(Out, "%" PRIuMAX "\n", uintmax_t(Val));
      fputs("*** Asts   ***\n", Out);
      TextWriter Writer;
      StartIndex = Asts.size() > WindowSize
          ? Asts.size() - WindowSize : 1;
      if (StartIndex > 1)
        fprintf(Out, "...[%" PRIuMAX "]\n", uintmax_t(Asts.size() - (StartIndex - 1)));
      for (const Node* Nd : Asts.iterRange(StartIndex))
        Writer.writeAbbrev(Out, Nd);
      fputs("**************\n", Out);
    });
#endif
#if 0
  PredefinedSymbol Name = Action->getPredefinedSymbol();
  switch (IntType(Name))
#else
  switch (Action)
#endif
  {
    case IntType(PredefinedSymbol::Binary_begin):
      // TODO(karlschimpf): Can we remove AstMarkers?
      AstMarkers.push(Asts.size());
      return true;
    case IntType(PredefinedSymbol::Binary_bit):
      switch (ValuesTop) {
        case 0:
          Values.push(OpBinaryAccept);
          return buildNullary<BinaryAcceptNode>();
        case 1:
          Values.push(OpBinaryEval);
          return buildBinary<BinarySelectNode>();
        default:
          fprintf(stderr, "Binary encoding Value not 0/1: %" PRIuMAX "\n",
                  uintmax_t(ValuesTop));
          return failWriteActionMalformed();
      }
      WASM_RETURN_UNREACHABLE(false);
    case IntType(PredefinedSymbol::Binary_end):
      Values.push(OpBinaryEval);
      return buildUnary<BinaryEvalNode>();
    case IntType(PredefinedSymbol::Int_value_begin):
      ValueMarker = Values.size();
      return true;
    case IntType(PredefinedSymbol::Int_value_end): {
      bool IsDefault;
      ValueFormat Format = ValueFormat::Decimal;
      IntType Value = 0;
      if (Values.size() < ValueMarker)
        return failWriteActionMalformed();
      switch (Values.size() - ValueMarker) {
        case 1:
          if (Values.popValue() != 0)
            return failWriteActionMalformed();
          IsDefault = true;
          break;
        case 2:
          IsDefault = false;
          Value = Values.popValue();
          Format = ValueFormat(Values.popValue() - 1);
          break;
        default:
          return failWriteActionMalformed();
      }
      Node* Nd = nullptr;
      switch (Values.popValue()) {
        case OpI32Const:
          Nd = IsDefault ? Symtab->getI32ConstDefinition()
                         : Symtab->getI32ConstDefinition(Value, Format);
          break;
        case OpI64Const:
          Nd = IsDefault ? Symtab->getI64ConstDefinition()
                         : Symtab->getI64ConstDefinition(Value, Format);
          break;
        case OpLocal:
          Nd = IsDefault ? Symtab->getLocalDefinition()
                         : Symtab->getLocalDefinition(Value, Format);
          break;
        case OpLocals:
          Nd = IsDefault ? Symtab->getLocalsDefinition()
                         : Symtab->getLocalsDefinition(Value, Format);
          break;
        case OpParam:
          Nd = IsDefault ? Symtab->getParamDefinition()
                         : Symtab->getParamDefinition(Value, Format);
          break;
        case OpParams:
          Nd = IsDefault ? Symtab->getParamsDefinition()
                         : Symtab->getParamsDefinition(Value, Format);
          break;
        case OpU8Const:
          Nd = IsDefault ? Symtab->getU8ConstDefinition()
                         : Symtab->getU8ConstDefinition(Value, Format);
          break;
        case OpU32Const:
          Nd = IsDefault ? Symtab->getU32ConstDefinition()
                         : Symtab->getU32ConstDefinition(Value, Format);
          break;
        case OpU64Const:
          Nd = IsDefault ? Symtab->getU64ConstDefinition()
                         : Symtab->getU64ConstDefinition(Value, Format);
          break;
        default:
          return failWriteActionMalformed();
      }
      TRACE(node_ptr, "Tree", Nd);
      Asts.push(Nd);
      return true;
    }
    case IntType(PredefinedSymbol::Symbol_name_begin):
      if (Values.empty())
        return failWriteActionMalformed();
      SymbolNameSize = Values.popValue();
      return true;
    case IntType(PredefinedSymbol::Symbol_name_end): {
      if (Values.size() < SymbolNameSize)
        return failWriteActionMalformed();
      // TODO(karlschimpf) Can we build the string faster than this.
      std::string Name;
      for (size_t i = Values.size() - SymbolNameSize; i < Values.size(); ++i)
        Name += char(Values[i]);
      for (size_t i = 0; i < SymbolNameSize; ++i)
        Values.pop();
      SymbolNameSize = 0;
      TRACE(string, "Name", Name);
      SectionSymtab->addSymbol(Name);
      return true;
    }
    case IntType(PredefinedSymbol::Symbol_lookup):
      if (Values.size() < 2)
        return failWriteActionMalformed();
      return applyOp(Values[Values.size() - 2]);
    case IntType(PredefinedSymbol::Postorder_inst):
      if (Values.size() < 1)
        return failWriteActionMalformed();
      return applyOp(ValuesTop);
    case IntType(PredefinedSymbol::Nary_inst):
      if (Values.size() < 2)
        return failWriteActionMalformed();
      TRACE(size_t, "nary node size", Values.size());
      return applyOp(Values[Values.size() - 2]);
    default:
      return Writer::writeAction(Action);
  }
  return false;
}

}  // end of namespace filt

}  // end of namespace wasm
