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

// Defines a converter of an Ast algorithm, to the corresponding
// (integer) CASM stream.

#include "sexp/FlattenAst.h"
#include "sexp/TextWriter.h"

namespace wasm {

using namespace decode;
using namespace interp;

namespace filt {

FlattenAst::FlattenAst(std::shared_ptr<IntWriter> Writer,
                       std::shared_ptr<SymbolTable> Symtab)
    : Writer(Writer),
      Symtab(Symtab),
      SectionSymtab(Symtab),
      FreezeEofOnDestruct(true),
      HasErrors(false) {
}

FlattenAst::~FlattenAst() {
  if (FreezeEofOnDestruct)
    Writer->writeFreezeEof();
}

void FlattenAst::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (!Trace)
    return;
  Trace->addContext(Writer->getTraceContext());
}

std::shared_ptr<TraceClassSexp> FlattenAst::getTracePtr() {
  return std::make_shared<TraceClassSexp>("FlattenAst");
}

void FlattenAst::reportError(const char* Message) {
  fprintf(stderr, "Error: %s\n", Message);
  HasErrors = true;
}

void FlattenAst::reportError(const char* Label, const Node* Nd) {
  fprintf(stderr, "%s: ", Label);
  TextWriter Writer;
  Writer.writeAbbrev(stderr, Nd);
  HasErrors = true;
}

void FlattenAst::writeNode(const Node* Nd) {
  if (HasErrors)
    return;
  TRACE_METHOD("writeNode");
  TRACE_SEXP(nullptr, Nd);
  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpUnknownSection: {
      reportError("Unexpected s-expression, can't write!");
      reportError("s-expression: ", Nd);
      break;
    }
#define X(tag, format, defval, mergable, NODE_DECLS) \
  case Op##tag: {                                    \
    Writer->write(Opcode);                           \
    auto* Int = cast<tag##Node>(Nd);                 \
    if (Int->isDefaultValue()) {                     \
      Writer->write(0);                              \
    } else {                                         \
      Writer->write(int(Int->getFormat()) + 1);      \
      Writer->write(Int->getValue());                \
    }                                                \
    break;                                           \
  }
      AST_INTEGERNODE_TABLE
#undef X
    case OpAnd:
    case OpBlock:
    case OpBitwiseAnd:
    case OpBitwiseNegate:
    case OpBitwiseOr:
    case OpBitwiseXor:
    case OpCallback:
    case OpCase:
    case OpConvert:
    case OpOr:
    case OpNot:
    case OpError:
    case OpIfThen:
    case OpIfThenElse:
    case OpLastSymbolIs:
    case OpLoop:
    case OpLoopUnbounded:
    case OpPeek:
    case OpRead:
    case OpUndefine:
    case OpLastRead:
    case OpRename:
    case OpSet:
    case OpLiteralDef:
    case OpLiteralUse:
    case OpVoid: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->write(Opcode);
      break;
    }
    case OpFile:
    case OpHeader: {
      // Note: The header appears at the beginning of the file, and hence,
      // isn't labeled.
      for (int i = 0; i < Nd->getNumKids(); ++i)
        writeNode(Nd->getKid(i));
      break;
    }
    case OpFileHeader: {
      for (const auto* Kid : *Nd) {
        TRACE_SEXP("Const", Kid);
        const auto* Const = dyn_cast<IntegerNode>(Kid);
        if (Const == nullptr) {
          reportError("Unrecognized literal constant", Nd);
          return;
        }
        if (!definesIntTypeFormat(Const)) {
          reportError("Bad literal constant", Const);
          return;
        }
        Writer->writeHeaderValue(Const->getType(), getIntTypeFormat(Const));
      }
      break;
    }
    case OpStream: {
      const auto* Stream = cast<StreamNode>(Nd);
      Writer->write(Opcode);
      Writer->write(Stream->getEncoding());
      break;
    }
    case OpSection: {
      Writer->writeAction(Symtab->getBlockEnterCallback());
      const auto* Section = cast<SectionNode>(Nd);
      SectionSymtab.installSection(Section);
      const SectionSymbolTable::IndexLookupType& Vector =
          SectionSymtab.getVector();
      Writer->write(Vector.size());
      for (const SymbolNode* Symbol : Vector) {
        const std::string& SymName = Symbol->getName();
        Writer->write(SymName.size());
        for (size_t i = 0, len = SymName.size(); i < len; ++i)
          Writer->write(SymName[i]);
      }
      for (int i = 0, len = Nd->getNumKids(); i < len; ++i)
        writeNode(Nd->getKid(i));
      Writer->writeAction(Symtab->getBlockExitCallback());
      SectionSymtab.clear();
      break;
    }
    case OpDefine:
    case OpEval:
    case OpFilter:
    case OpOpcode:
    case OpMap:
    case OpSwitch:
    case OpSequence:
    case OpWrite: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->write(Opcode);
      Writer->write(Nd->getNumKids());
      break;
    }
    case OpSymbol: {
      Writer->write(Opcode);
      SymbolNode* Sym = cast<SymbolNode>(const_cast<Node*>(Nd));
      Writer->write(SectionSymtab.getSymbolIndex(Sym));
      break;
    }
  }
}

}  // end of namespace filt

}  // end of namespace wasm
