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

#include "casm/FlattenAst.h"

#include <algorithm>

#include "binary/SectionSymbolTable.h"
#include "interp/IntWriter.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "utils/Casting.h"
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
      SectionSymtab(utils::make_unique<SectionSymbolTable>(Symtab)),
      FreezeEofOnDestruct(true),
      HasErrors(false),
      WrotePrimaryHeader(false),
      BitCompress(false) {
}

FlattenAst::~FlattenAst() {
  freezeOutput();
}

bool FlattenAst::flatten(bool BitCompressValue) {
  BitCompress = BitCompressValue;
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

void FlattenAst::reportError(charstring Message) {
  fprintf(stderr, "Error: %s\n", Message);
  HasErrors = true;
}

void FlattenAst::reportError(charstring Label, const Node* Nd) {
  fprintf(stderr, "%s: ", Label);
  TextWriter Writer;
  Writer.writeAbbrev(stderr, Nd);
  HasErrors = true;
}

bool FlattenAst::binaryEvalEncode(const BinaryEvalNode* Nd) {
  TRACE_METHOD("binaryEValEncode");
  // Build a (reversed) postorder sequence of nodes, and then reverse.
  std::vector<uint8_t> PostorderEncoding;
  std::vector<Node*> Frontier;
  Frontier.push_back(Nd->getKid(0));
  while (!Frontier.empty()) {
    Node* Nd = Frontier.back();
    Frontier.pop_back();
    switch (Nd->getType()) {
      default:
        // Not suitable for bit encoding.
        return false;
      case OpBinarySelect:
        PostorderEncoding.push_back(1);
        break;
      case OpBinaryAccept:
        PostorderEncoding.push_back(0);
        break;
    }
    for (int i = 0; i < Nd->getNumKids(); ++i)
      Frontier.push_back(Nd->getKid(i));
  }
  std::reverse(PostorderEncoding.begin(), PostorderEncoding.end());
  // Can bit encode tree (1 => BinaryEvalNode, 0 => BinaryAcceptNode).
  Writer->write(OpBinaryEvalBits);
  TRACE(size_t, "NumBIts", PostorderEncoding.size());
  Writer->write(PostorderEncoding.size());
  for (uint8_t Val : PostorderEncoding) {
    TRACE(uint8_t, "bit", Val);
    Writer->writeBit(Val);
  }
  return true;
}

void FlattenAst::flattenNode(const Node* Nd) {
  if (HasErrors)
    return;
  TRACE_METHOD("flattenNode");
  TRACE(node_ptr, nullptr, Nd);
  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpBinaryEvalBits:
    case OpIntLookup:
    case OpSymbolDefn:
    case OpUnknownSection: {
      reportError("Unexpected s-expression, can't write!");
      reportError("s-expression: ", Nd);
      break;
    }
#define X(tag, format, defval, mergable, BASE, NODE_DECLS) \
  case Op##tag: {                                          \
    Writer->write(Opcode);                                 \
    auto* Int = cast<tag##Node>(Nd);                       \
    if (Int->isDefaultValue()) {                           \
      Writer->write(0);                                    \
    } else {                                               \
      Writer->write(int(Int->getFormat()) + 1);            \
      Writer->write(Int->getValue());                      \
    }                                                      \
    break;                                                 \
  }
      AST_INTEGERNODE_TABLE
#undef X
    case OpBinaryEval:
      if (BitCompress && binaryEvalEncode(cast<BinaryEvalNode>(Nd)))
        break;
    // Not binary encoding, Intentionally fall to next case that
    // will generate non-compressed form.
    case OpAnd:
    case OpBlock:
    case OpBinaryAccept:
    case OpBinarySelect:
    case OpBit:
    case OpBitwiseAnd:
    case OpBitwiseNegate:
    case OpBitwiseOr:
    case OpBitwiseXor:
    case OpCallback:
    case OpCase:
    case OpError:
    case OpIfThen:
    case OpIfThenElse:
    case OpLastRead:
    case OpLastSymbolIs:
    case OpLiteralActionDef:
    case OpLiteralActionUse:
    case OpLiteralDef:
    case OpLiteralUse:
    case OpLoop:
    case OpLoopUnbounded:
    case OpNot:
    case OpOr:
    case OpPeek:
    case OpRead:
    case OpRename:
    case OpSet:
    case OpTable:
    case OpUint32:
    case OpUint64:
    case OpUint8:
    case OpUndefine:
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
      int NumKids = Nd->getNumKids();
      if (NumKids <= 1)
        return reportError("No source header defined for algorithm");
      // Write primary header. Note: Only the constants are written out (See
      // case OpFileHeader). The reader will automatically build the
      // corresponding AST while reading the constants.
      flattenNode(Nd->getKid(0));

      // Write out other headers. However, first write out the total number
      // of nodes in the ast's, so that the header nodes will be read and
      // built by the inflator.
      size_t NumHeaderNodes = 0;
      for (int i = 1; i < NumKids - 1; ++i)
        NumHeaderNodes += Nd->getKid(i)->getTreeSize();
      TRACE(size_t, "HeaderSize", NumHeaderNodes);
      Writer->write(NumHeaderNodes);

      // Now flatten remaining kids.
      for (int i = 1; i < NumKids; ++i)
        flattenNode(Nd->getKid(i));
      Writer->write(Opcode);
      Writer->write(NumKids);
      break;
    }
    case OpFileHeader: {
#if 0
      if (WrotePrimaryHeader) {
        // Must be secondary header. write out as ordinary nary node.
        for (const auto* Kid : *Nd)
          flattenNode(Kid);
        Writer->write(Opcode);
        Writer->write(Nd->getNumKids());
        break;
      }
#endif
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
      // Intentionally drop to next case.
    }
    case OpReadHeader:
    case OpWriteHeader:
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
      Writer->write(Opcode);
      Writer->write(Nd->getNumKids());
      break;
    case OpSection: {
      Writer->writeAction(IntType(PredefinedSymbol::Block_enter));
      const auto* Section = cast<SectionNode>(Nd);
      SectionSymtab->installSection(Section);
      const SectionSymbolTable::IndexLookupType& Vector =
          SectionSymtab->getVector();
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
      Writer->write(Nd->getNumKids());
      Writer->writeAction(IntType(PredefinedSymbol::Block_exit));
      SectionSymtab->clear();
      break;
    }
    case OpDefine:
    case OpEval:
    case OpLiteralActionBase:
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
      Writer->write(SectionSymtab->getSymbolIndex(Sym));
      break;
    }
  }
}

}  // end of namespace filt

}  // end of namespace wasm
