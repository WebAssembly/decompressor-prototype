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

#include "sexp/InflateAst.h"
#include "sexp/TextWriter.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

InflateAst::InflateAst()
    : Symtab(std::make_shared<SymbolTable>()),
      SectionSymtab(Symtab),
      ValuesTop(0),
      Values(ValuesTop),
      Ast(nullptr),
      AstStack(Ast),
      SymbolNameSize(0),
      ValueMarker(0),
      AstMarkerTop(0),
      AstMarkerStack(AstMarkerTop) {
}

InflateAst::~InflateAst() {
}

FileNode* InflateAst::getGeneratedFile() const {
  if (AstStack.size() != 1)
    return nullptr;
  return dyn_cast<FileNode>(Ast);
}

template <class T>
bool InflateAst::buildNullary() {
  Values.pop();
  AstStack.push(Symtab->create<T>());
  TRACE_SEXP(nullptr, Ast);
  return true;
}

template <class T>
bool InflateAst::buildUnary() {
  Values.pop();
  AstStack.push(Symtab->create<T>(AstStack.popValue()));
  TRACE_SEXP(nullptr, Ast);
  return true;
}

template <class T>
bool InflateAst::buildBinary() {
  Values.pop();
  Node* Arg2 = AstStack.popValue();
  Node* Arg1 = AstStack.popValue();
  AstStack.push(Symtab->create<T>(Arg1, Arg2));
  TRACE_SEXP(nullptr, Ast);
  return true;
}

bool InflateAst::appendArgs(Node* Nd) {
  size_t NumArgs = Values.popValue();
  Values.pop();
  for (size_t i = AstStack.size() - NumArgs; i < AstStack.size(); ++i)
    Nd->append(AstStack[i]);
  for (size_t i = 0; i < NumArgs; ++i)
    AstStack.pop();
  TRACE_SEXP(nullptr, Nd);
  AstStack.push(Nd);
  return true;
}

template <class T>
bool InflateAst::buildNary() {
  return appendArgs(Symtab->create<T>());
}

// TODO(karlschimpf) Should we extend StreamType to have other?
StreamType InflateAst::getStreamType() const {
  return StreamType::Int;
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

bool InflateAst::writeValue(decode::IntType Value, const filt::Node*) {
  return write(Value);
}

bool InflateAst::writeTypedValue(decode::IntType Value, interp::IntTypeFormat) {
  return write(Value);
}

bool InflateAst::writeHeaderValue(decode::IntType Value,
                                  interp::IntTypeFormat Format) {
  if (AstStack.empty())
    AstStack.push(Symtab->create<FileHeaderNode>());
  if (AstStack.size() != 1)
    return false;
  Node* Header = Ast;
  switch (Format) {
    case IntTypeFormat::Uint8:
      AstStack.push(
          Symtab->getU8ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint32:
      AstStack.push(
          Symtab->getU32ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint64:
      AstStack.push(
          Symtab->getU64ConstDefinition(Value, ValueFormat::Hexidecimal));
      break;
    default:
      return false;
  }
  TRACE_SEXP("Header", Ast);
  Header->append(AstStack.popValue());
  return true;
}

bool InflateAst::applyOp(IntType Op) {
  TRACE_METHOD("applyOp");
  TRACE(IntType, "Op", Op);
  switch (NodeType(Op)) {
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
    case OpIfThen:
      return buildBinary<IfThenNode>();
    case OpLiteralDef:
      return buildBinary<LiteralDefNode>();
    case OpLiteralUse:
      return buildUnary<LiteralUseNode>();
    case OpLoop:
      return buildBinary<LoopNode>();
    case OpLoopUnbounded:
      return buildUnary<LoopUnboundedNode>();
    case OpRead:
      return buildUnary<ReadNode>();
    case OpSection:
      // Note: Bottom element is for file.
      TRACE(size_t, "Stack.size", AstStack.size());
      if (AstStack.empty())
        return false;
      Values.push(AstStack.size() - 1);
      if (!buildNary<SectionNode>())
        return false;
      Values.push(OpFile);
      return buildBinary<FileNode>();
    case OpSequence:
      return buildNary<SequenceNode>();
    case OpSwitch:
      return buildNary<SwitchNode>();
    case OpSymbol: {
      SymbolNode* Sym = SectionSymtab.getIndexSymbol(Values.popValue());
      Values.pop();
      TRACE_SEXP(nullptr, Sym);
      if (Sym == nullptr)
        return false;
      AstStack.push(Sym);
      return true;
    }
    default:
      TRACE_MESSAGE("unknonw");
      break;
  }
  return false;
}

bool InflateAst::writeAction(const filt::CallbackNode* Action) {
  TRACE_SEXP("Action", Action);
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
      StartIndex = AstStack.size() > WindowSize
          ? AstStack.size() - WindowSize : 1;
      if (StartIndex > 1)
        fprintf(Out, "...[%" PRIuMAX "]\n", uintmax_t(AstStack.size() - (StartIndex - 1)));
      for (const Node* Nd : AstStack.iterRange(StartIndex))
        Writer.writeAbbrev(Out, Nd);
      fputs("**************\n", Out);
    });
#endif
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  PredefinedSymbol Name = Sym->getPredefinedSymbol();
  switch (Name) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_exit:
      return true;
    case PredefinedSymbol::Instruction_begin:
      AstMarkerStack.push(AstStack.size());
      return false;
    case PredefinedSymbol::Int_value_begin:
      ValueMarker = Values.size();
      return true;
    case PredefinedSymbol::Int_value_end: {
      bool IsDefault;
      ValueFormat Format = ValueFormat::Decimal;
      IntType Value = 0;
      if (Values.size() < ValueMarker)
        return false;
      switch (Values.size() - ValueMarker) {
        case 1:
          if (Values.popValue() != 0)
            return false;
          IsDefault = true;
          break;
        case 2:
          IsDefault = false;
          Value = Values.popValue();
          Format = ValueFormat(Values.popValue() - 1);
          break;
        default:
          return false;
      }
      Node* Nd = nullptr;
      TRACE(IntType, "Op", ValuesTop);
      switch (Values.popValue()) {
        case OpUint8:
          Nd = IsDefault ? Symtab->getUint8Definition()
                         : Symtab->getUint8Definition(Value, Format);
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
        case OpVarint32:
          Nd = IsDefault ? Symtab->getVarint32Definition()
                         : Symtab->getVarint32Definition(Value, Format);
          break;
        case OpVarint64:
          Nd = IsDefault ? Symtab->getVarint64Definition()
                         : Symtab->getVarint64Definition(Value, Format);
          break;
        case OpVaruint32:
          Nd = IsDefault ? Symtab->getVaruint32Definition()
                         : Symtab->getVaruint32Definition(Value, Format);
          break;
        case OpVaruint64:
          Nd = IsDefault ? Symtab->getVaruint64Definition()
                         : Symtab->getVaruint64Definition(Value, Format);
          break;
        default:
          return false;
      }
      AstStack.push(Nd);
      return true;
    }
    case PredefinedSymbol::Literal_define: {
      if (AstStack.size() < 2)
        return false;
      Node* Arg2 = AstStack.popValue();
      Node* Arg1 = AstStack.popValue();
      AstStack.push(Symtab->create<LiteralDefNode>(Arg1, Arg2));
      TRACE_SEXP("Define", Ast);
      return false;
    }
    case PredefinedSymbol::Symbol_name_begin:
      if (Values.empty())
        return false;
      SymbolNameSize = Values.popValue();
      return true;
    case PredefinedSymbol::Symbol_name_end: {
      if (Values.size() < SymbolNameSize)
        return false;
      // TODO(karlschimpf) Can we build the string faster than this.
      std::string Name;
      for (size_t i = Values.size() - SymbolNameSize; i < Values.size(); ++i)
        Name += char(Values[i]);
      for (size_t i = 0; i < SymbolNameSize; ++i)
        Values.pop();
      SymbolNameSize = 0;
      TRACE(string, "Name", Name);
      SectionSymtab.addSymbol(Name);
      return true;
    }
    case PredefinedSymbol::Symbol_lookup:
      if (Values.size() < 2)
        return false;
      return applyOp(Values[Values.size() - 2]);
    case PredefinedSymbol::Postorder_inst:
      if (Values.size() < 1)
        return false;
      return applyOp(ValuesTop);
    case PredefinedSymbol::Nary_inst:
      if (Values.size() < 2)
        return false;
      TRACE(size_t, "Values.size", Values.size());
      return applyOp(Values[Values.size() - 2]);
      return false;
    default:
      break;
  }
  return false;
}

}  // end of namespace filt

}  // end of namespace wasm
