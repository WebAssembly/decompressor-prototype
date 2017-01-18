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
#include "utils/Trace.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

FlattenAst::FlattenAst(std::shared_ptr<IntStream> Output,
                       std::shared_ptr<SymbolTable> Symtab)
    : Writer(std::make_shared<IntWriter>(Output)),
      Symtab(Symtab),
      SectionSymtab(Symtab),
      FreezeEofOnDestruct(true),
      HasErrors(false),
      WrotePrimaryHeader(false) {
}

FlattenAst::~FlattenAst() {
  freezeOutput();
}

bool FlattenAst::flatten() {
  flattenNode(Symtab->getInstalledRoot());
  freezeOutput();
  return !HasErrors;
}

void FlattenAst::freezeOutput() {
  if (!FreezeEofOnDestruct)
    return;
  FreezeEofOnDestruct = false;
  Writer->writeFreezeEof();
}

void FlattenAst::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  getTrace().setTraceProgress(NewValue);
}

void FlattenAst::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (!Trace)
    return;
  Trace->addContext(Writer->getTraceContext());
  TRACE_MESSAGE("Trace started");
}

std::shared_ptr<TraceClass> FlattenAst::getTracePtr() {
  if (!Trace) {
    setTrace(std::make_shared<TraceClass>("FlattenAst"));
  }
  return Trace;
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

void FlattenAst::flattenNode(const Node* Nd) {
  if (HasErrors)
    return;
  TRACE_METHOD("flattenNode");
  TRACE(node_ptr, nullptr, Nd);
  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpBinarySelect:
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
    case OpUint32:
    case OpUint64:
    case OpUint8:
    case OpVarint32:
    case OpVarint64:
    case OpVaruint32:
    case OpVaruint64:
    case OpVoid: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
      Writer->write(Opcode);
      break;
    }
    case OpFile: {
      assert(Nd->getNumKids() == 3);
      // Write primary heder.
      flattenNode(Nd->getKid(0));
      // Before secondary header, write out tree size so that reader knows
      // how many nodes to read.
      TRACE(size_t, "TreeSize", Nd->getKid(1)->getTreeSize());
      Writer->write(Nd->getKid(1)->getTreeSize());
      flattenNode(Nd->getKid(1));
      flattenNode(Nd->getKid(2));
      break;
    }
    case OpFileHeader: {
      if (WrotePrimaryHeader) {
        // Must be secondary header. write out as ordinary nary node.
        for (const auto* Kid : *Nd)
          flattenNode(Kid);
        Writer->write(Opcode);
        Writer->write(Nd->getNumKids());
        break;
      }
      // The primary header is special in that the size is defined by the
      // reading algorithm, and no "FileHeader" opcode is generated.
      for (const auto* Kid : *Nd) {
        TRACE(node_ptr, "Const", Kid);
        const auto* Const = dyn_cast<IntegerNode>(Kid);
        if (Const == nullptr) {
          reportError("Unrecognized literal constant", Nd);
          return;
        }
        if (!Const->definesIntTypeFormat()) {
          reportError("Bad literal constant", Const);
          return;
        }
        Writer->writeHeaderValue(Const->getValue(), Const->getIntTypeFormat());
      }
      WrotePrimaryHeader = true;
      break;
    }
    case OpSection: {
      Writer->writeAction(Symtab->getPredefined(PredefinedSymbol::Block_enter));
      const auto* Section = cast<SectionNode>(Nd);
      SectionSymtab.installSection(Section);
      const SectionSymbolTable::IndexLookupType& Vector =
          SectionSymtab.getVector();
      Writer->write(Vector.size());
      TRACE(size_t, "Number symbols", Vector.size());
      for (const SymbolNode* Symbol : Vector) {
        const std::string& SymName = Symbol->getName();
        TRACE(string, "Symbol", SymName);
        Writer->write(SymName.size());
        for (size_t i = 0, len = SymName.size(); i < len; ++i)
          Writer->write(SymName[i]);
      }
      for (int i = 0, len = Nd->getNumKids(); i < len; ++i)
        flattenNode(Nd->getKid(i));
      Writer->writeUint8(Opcode);
      Writer->writeAction(Symtab->getPredefined(PredefinedSymbol::Block_exit));
      SectionSymtab.clear();
      break;
    }
    case OpDefine:
    case OpEval:
    case OpOpcode:
    case OpMap:
    case OpSwitch:
    case OpSequence:
    case OpWrite: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
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
