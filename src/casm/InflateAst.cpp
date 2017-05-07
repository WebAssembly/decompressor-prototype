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

#include "casm/SymbolIndex.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "utils/Casting.h"
#include "utils/Trace.h"

#define DEBUG_FILE 0

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

InflateAst::InflateAst()
    : Writer(false),
      Symtab(std::make_shared<SymbolTable>()),
      SymIndex(utils::make_unique<SymbolIndex>(Symtab)),
      ValuesTop(0),
      Values(ValuesTop),
      AstsTop(nullptr),
      Asts(AstsTop),
      SymbolNameSize(0),
      ValueMarker(0),
      AstMarkersTop(0),
      AstMarkers(AstMarkersTop),
      InstallDuringInflation(true) {}

InflateAst::~InflateAst() {}

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

Algorithm* InflateAst::getGeneratedFile() const {
  if (Asts.size() != 1)
    return nullptr;
  return dyn_cast<Algorithm>(AstsTop);
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

bool InflateAst::write(decode::IntType Value) {
  TRACE(IntType, "writeValue", Value);
  Values.push(Value);
  return true;
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
    Asts.push(Symtab->create<SourceHeader>());
  if (Asts.size() != 1)
    return failWriteHeaderMalformed();
  Node* Header = AstsTop;
  switch (Format) {
    case IntTypeFormat::Uint8:
      Asts.push(Symtab->create<U8Const>(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint32:
      Asts.push(Symtab->create<U32Const>(Value, ValueFormat::Hexidecimal));
      break;
    case IntTypeFormat::Uint64:
      Asts.push(Symtab->create<U64Const>(Value, ValueFormat::Hexidecimal));
      break;
    default:
      return failWriteHeaderMalformed();
  }
  Header->append(Asts.popValue());
  return true;
}

bool InflateAst::applyOp(IntType Op) {
  // Handle special cases first.
  switch (NodeType(Op)) {
    default:
      break;
    case NodeType::Algorithm: {
      Algorithm* Alg = buildNary<Algorithm>() ? getGeneratedFile() : nullptr;
      if (Alg == nullptr)
        return failBuild("InflateAst", "Unable to read (inflate) algorithm");
      Symtab->setAlgorithm(Alg);
      if (InstallDuringInflation)
        Symtab->install();
      return true;
    }
    case NodeType::Symbol: {
      Symbol* Sym = SymIndex->getIndexSymbol(Values.popValue());
      Values.pop();
      if (Sym == nullptr) {
        return failWriteActionMalformed();
      }
      TRACE(node_ptr, "Tree", Sym);
      Asts.push(Sym);
      return true;
    }
  }

  // Now handle default cases.
  switch (NodeType(Op)) {
#define X(NAME, FORMAT, DEFAULT, MERGE, BASE, DECLS, INIT) case NodeType::NAME:
    AST_INTEGERNODE_TABLE
#undef X
#define X(NAME) case NodeType::NAME:
    AST_CACHEDNODE_TABLE
#undef X
    case NodeType::BinaryEvalBits:
    case NodeType::Opcode:
    case NodeType::Symbol:
    case NodeType::NO_SUCH_NODETYPE:
      return failWriteActionMalformed();

#define X(NAME, BASE, VALUE, FORMAT, DECLS, INIT) \
  case NodeType::NAME:                            \
    return buildNullary<NAME>();
      AST_LITERAL_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildNullary<NAME>();
      AST_NULLARYNODE_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildUnary<NAME>();
      AST_UNARYNODE_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildBinary<NAME>();
      AST_BINARYNODE_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildNary<NAME>();
      AST_NARYNODE_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildNary<NAME>();
      AST_SELECTNODE_TABLE
#undef X
#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return buildTernary<NAME>();
      AST_TERNARYNODE_TABLE
#undef X
    case NodeType::BinaryAccept:
      return buildNullary<BinaryAccept>();
    case NodeType::BinaryEval:
      return buildUnary<BinaryEval>();
  }
  return failWriteActionMalformed();
}

bool InflateAst::writeAction(IntType Action) {
#if DEBUG_FILE
  TRACE_BLOCK({
    constexpr size_t WindowSize = 10;
    FILE* Out = getTrace().getFile();
    fputs("*** Values ***\n", Out);
    size_t StartIndex =
        Values.size() > WindowSize ? Values.size() - WindowSize : 1;
    if (StartIndex > 1)
      fprintf(Out, "...[%" PRIuMAX "]\n",
              uintmax_t(Values.size() - (StartIndex - 1)));
    for (IntType Val : Values.iterRange(StartIndex))
      fprintf(Out, "%" PRIuMAX "\n", uintmax_t(Val));
    fputs("*** Asts   ***\n", Out);
    TextWriter Writer;
    StartIndex = Asts.size() > WindowSize ? Asts.size() - WindowSize : 1;
    if (StartIndex > 1)
      fprintf(Out, "...[%" PRIuMAX "]\n",
              uintmax_t(Asts.size() - (StartIndex - 1)));
    for (const Node* Nd : Asts.iterRange(StartIndex))
      Writer.writeAbbrev(Out, Nd);
    fputs("**************\n", Out);
  });
#endif
  switch (Action) {
    case IntType(PredefinedSymbol::Binary_begin):
      // TODO(karlschimpf): Can we remove AstMarkers?
      AstMarkers.push(Asts.size());
      return true;
    case IntType(PredefinedSymbol::Binary_bit):
      switch (ValuesTop) {
        case 0:
          Values.push(IntType(NodeType::BinaryAccept));
          return buildNullary<BinaryAccept>();
        case 1:
          Values.push(IntType(NodeType::BinaryEval));
          return buildBinary<BinarySelect>();
        default:
          fprintf(stderr, "Binary encoding Value not 0/1: %" PRIuMAX "\n",
                  uintmax_t(ValuesTop));
          return failWriteActionMalformed();
      }
      WASM_RETURN_UNREACHABLE(false);
    case IntType(PredefinedSymbol::Binary_end):
      Values.push(IntType(NodeType::BinaryEval));
      return buildUnary<BinaryEval>();
    case IntType(PredefinedAlgcasm0x0::Int_value_begin):
      ValueMarker = Values.size();
      return true;
    case IntType(PredefinedAlgcasm0x0::Int_value_end): {
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
      switch (NodeType(Values.popValue())) {
        case NodeType::I32Const:
          Nd = IsDefault ? Symtab->create<I32Const>()
                         : Symtab->create<I32Const>(Value, Format);
          break;
        case NodeType::I64Const:
          Nd = IsDefault ? Symtab->create<I64Const>()
                         : Symtab->create<I64Const>(Value, Format);
          break;
        case NodeType::Local:
          Nd = IsDefault ? Symtab->create<Local>()
                         : Symtab->create<Local>(Value, Format);
          break;
        case NodeType::Locals:
          Nd = IsDefault ? Symtab->create<Locals>()
                         : Symtab->create<Locals>(Value, Format);
          break;
        case NodeType::Param:
          Nd = IsDefault ? Symtab->create<Param>()
                         : Symtab->create<Param>(Value, Format);
          break;
        case NodeType::ParamExprs:
          Nd = IsDefault ? Symtab->create<ParamExprs>()
                         : Symtab->create<ParamExprs>(Value, Format);
          break;
        case NodeType::ParamValues:
          Nd = IsDefault ? Symtab->create<ParamValues>()
                         : Symtab->create<ParamValues>(Value, Format);
          break;
        case NodeType::U8Const:
          Nd = IsDefault ? Symtab->create<U8Const>()
                         : Symtab->create<U8Const>(Value, Format);
          break;
        case NodeType::U32Const:
          Nd = IsDefault ? Symtab->create<U32Const>()
                         : Symtab->create<U32Const>(Value, Format);
          break;
        case NodeType::U64Const:
          Nd = IsDefault ? Symtab->create<U64Const>()
                         : Symtab->create<U64Const>(Value, Format);
          break;
        default:
          return failWriteActionMalformed();
      }
      TRACE(node_ptr, "Tree", Nd);
      Asts.push(Nd);
      return true;
    }
    case IntType(PredefinedAlgcasm0x0::Symbol_name_begin):
      if (Values.empty())
        return failWriteActionMalformed();
      SymbolNameSize = Values.popValue();
      return true;
    case IntType(PredefinedAlgcasm0x0::Symbol_name_end): {
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
      SymIndex->addSymbol(Name);
      return true;
    }
    case IntType(PredefinedAlgcasm0x0::Symbol_lookup):
      if (Values.size() < 2)
        return failWriteActionMalformed();
      return applyOp(Values[Values.size() - 2]);
    case IntType(PredefinedAlgcasm0x0::Postorder_inst):
      if (Values.size() < 1)
        return failWriteActionMalformed();
      return applyOp(ValuesTop);
    case IntType(PredefinedAlgcasm0x0::Nary_inst):
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
