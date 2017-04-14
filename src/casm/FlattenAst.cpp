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
      BitCompress(false) {
}

FlattenAst::~FlattenAst() {
  freezeOutput();
}

bool FlattenAst::flatten(bool BitCompressValue) {
  BitCompress = BitCompressValue;
  flattenNode(Symtab->getAlgorithm());
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

bool FlattenAst::binaryEvalEncode(const BinaryEval* Nd) {
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
      case NodeType::BinarySelect:
        PostorderEncoding.push_back(1);
        break;
      case NodeType::BinaryAccept:
        PostorderEncoding.push_back(0);
        break;
    }
    for (int i = 0; i < Nd->getNumKids(); ++i)
      Frontier.push_back(Nd->getKid(i));
  }
  std::reverse(PostorderEncoding.begin(), PostorderEncoding.end());
  // Can bit encode tree (1 => BinaryEvalNode, 0 => BinaryAcceptNode).
  Writer->write(IntType(NodeType::BinaryEvalBits));
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
    case NodeType::NO_SUCH_NODETYPE:
    case NodeType::BinaryEvalBits:
    case NodeType::IntLookup:
    case NodeType::SymbolDefn:
    case NodeType::UnknownSection: {
      reportError("Unexpected s-expression, can't write!");
      reportError("s-expression: ", Nd);
      break;
    }
#define X(tag, format, defval, mergable, BASE, NODE_DECLS) \
  case NodeType::tag: {                                    \
    Writer->write(IntType(Opcode));                        \
    auto* Int = cast<tag>(Nd);                             \
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
    case NodeType::BinaryEval:
      if (BitCompress && binaryEvalEncode(cast<BinaryEval>(Nd)))
        break;
    // Not binary encoding, Intentionally fall to next case that
    // will generate non-compressed form.
    case NodeType::And:
    case NodeType::Block:
    case NodeType::BinaryAccept:
    case NodeType::BinarySelect:
    case NodeType::Bit:
    case NodeType::BitwiseAnd:
    case NodeType::BitwiseNegate:
    case NodeType::BitwiseOr:
    case NodeType::BitwiseXor:
    case NodeType::Callback:
    case NodeType::Case:
    case NodeType::Error:
    case NodeType::IfThen:
    case NodeType::IfThenElse:
    case NodeType::LastRead:
    case NodeType::LastSymbolIs:
    case NodeType::LiteralActionDef:
    case NodeType::LiteralActionUse:
    case NodeType::LiteralDef:
    case NodeType::LiteralUse:
    case NodeType::Loop:
    case NodeType::LoopUnbounded:
    case NodeType::Not:
    case NodeType::Or:
    case NodeType::Peek:
    case NodeType::Read:
    case NodeType::Rename:
    case NodeType::Set:
    case NodeType::Table:
    case NodeType::Uint32:
    case NodeType::Uint64:
    case NodeType::Uint8:
    case NodeType::Undefine:
    case NodeType::Varint32:
    case NodeType::Varint64:
    case NodeType::Varuint32:
    case NodeType::Varuint64:
    case NodeType::Void: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
      Writer->write(IntType(Opcode));
      break;
    }
    case NodeType::Algorithm: {
      int NumKids = Nd->getNumKids();
      if (NumKids <= 1)
        return reportError("No source header defined for algorithm");
      // Write primary header. Note: Only the constants are written out (See
      // case NodeType::FileHeader). The reader will automatically build the
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
      Writer->write(IntType(Opcode));
      Writer->write(NumKids);
      break;
    }
    case NodeType::SourceHeader: {
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
      break;
      // Intentionally drop to next case.
    }
    case NodeType::ReadHeader:
    case NodeType::WriteHeader:
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
      Writer->write(IntType(Opcode));
      Writer->write(Nd->getNumKids());
      break;
    case NodeType::Section: {
      Writer->writeAction(IntType(PredefinedSymbol::Block_enter));
      const auto* Sec = cast<Section>(Nd);
      SectionSymtab->installSection(Sec);
      const SectionSymbolTable::IndexLookupType& Vector =
          SectionSymtab->getVector();
      Writer->write(Vector.size());
      TRACE(size_t, "Number symbols", Vector.size());
      for (const Symbol* Sym : Vector) {
        const std::string& SymName = Sym->getName();
        TRACE(string, "Symbol", SymName);
        Writer->write(SymName.size());
        for (size_t i = 0, len = SymName.size(); i < len; ++i)
          Writer->write(SymName[i]);
      }
      for (int i = 0, len = Nd->getNumKids(); i < len; ++i)
        flattenNode(Nd->getKid(i));
      Writer->writeUint8(IntType(Opcode));
      Writer->write(Nd->getNumKids());
      Writer->writeAction(IntType(PredefinedSymbol::Block_exit));
      SectionSymtab->clear();
      break;
    }
    case NodeType::Define:
    case NodeType::Eval:
    case NodeType::LiteralActionBase:
    case NodeType::Opcode:
    case NodeType::Map:
    case NodeType::Switch:
    case NodeType::Sequence:
    case NodeType::Write: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        flattenNode(Kid);
      Writer->write(IntType(Opcode));
      Writer->write(Nd->getNumKids());
      break;
    }
    case NodeType::Symbol: {
      Writer->write(IntType(Opcode));
      Symbol* Sym = cast<Symbol>(const_cast<Node*>(Nd));
      Writer->write(SectionSymtab->getSymbolIndex(Sym));
      break;
    }
  }
}

}  // end of namespace filt

}  // end of namespace wasm
