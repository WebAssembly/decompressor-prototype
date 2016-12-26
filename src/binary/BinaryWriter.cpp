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
//

// Implements a binary generator for filter s-expressions.

#include "binary/BinaryWriter.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "interp/ByteWriteStream.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

BinaryWriter::BinaryWriter(std::shared_ptr<decode::Queue> Output,
                           std::shared_ptr<SymbolTable> Symtab)
    : WritePos(decode::StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      SectionSymtab(Symtab),
      MinimizeBlockSize(false),
      FreezeEofOnDestruct(true) {
}

void BinaryWriter::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  getTrace().setTraceProgress(NewValue);
}

void BinaryWriter::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (!Trace)
    return;
  Trace->addContext(WritePos.getTraceContext());
}

std::shared_ptr<utils::TraceClass> BinaryWriter::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("BinaryWriter"));
  return Trace;
}

BinaryWriter::~BinaryWriter() {
  if (FreezeEofOnDestruct)
    WritePos.freezeEof();
}

void BinaryWriter::writeLiteral(const Node* Nd) {
  TRACE_METHOD("writeliteral");
  TRACE(node_ptr, nullptr, Nd);
  const auto* Const = dyn_cast<IntegerNode>(Nd);
  if (Const == nullptr)
    fatal("Unrecognized literal constant!");
  switch (Const->getType()) {
    default:
      fatal("Unrecognized literal constant!");
      break;
    case OpU8Const:
      Writer->writeUint8(cast<U8ConstNode>(Const)->getValue(), WritePos);
      break;
    case OpI32Const:
      Writer->writeInt32(cast<I32ConstNode>(Const)->getValue(), WritePos);
      break;
    case OpU32Const:
      Writer->writeUint32(cast<U32ConstNode>(Const)->getValue(), WritePos);
      break;
    case OpI64Const:
      Writer->writeInt64(cast<I64ConstNode>(Const)->getValue(), WritePos);
      break;
    case OpU64Const:
      Writer->writeUint64(cast<U64ConstNode>(Const)->getValue(), WritePos);
      break;
  }
}

void BinaryWriter::writeNode(const Node* Nd) {
  TRACE_METHOD("writeNode");
  TRACE(node_ptr, nullptr, Nd);
  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpUnknownSection: {
      fprintf(stderr, "Misplaced s-expression: %s\n", getNodeTypeName(Opcode));
      fatal("Unable to write filter s-expression");
      break;
    }
#define X(tag, format, defval, mergable, NODE_DECLS)           \
  case Op##tag: {                                              \
    Writer->writeUint8(Opcode, WritePos);                      \
    auto* Int = cast<tag##Node>(Nd);                           \
    if (Int->isDefaultValue()) {                               \
      Writer->writeUint8(0, WritePos);                         \
    } else {                                                   \
      Writer->writeUint8(int(Int->getFormat()) + 1, WritePos); \
      Writer->write##format(Int->getValue(), WritePos);        \
    }                                                          \
    break;                                                     \
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
      Writer->writeUint8(Opcode, WritePos);
      break;
    }
    case OpFile: {
      // Note: The header appears at the beginning of the file, and hence,
      // isn't labeled.
      for (int i = 0; i < Nd->getNumKids(); ++i)
        writeNode(Nd->getKid(i));
      break;
    }
    case OpFileHeader: {
      for (int i = 0; i < Nd->getNumKids(); ++i)
        writeLiteral(Nd->getKid(i));
      break;
    }
    case OpStream: {
      const auto* Stream = cast<StreamNode>(Nd);
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeUint8(Stream->getEncoding(), WritePos);
      break;
    }
    case OpSection: {
      writeBlock([&]() {
        const auto* Section = cast<SectionNode>(Nd);
        SectionSymtab.installSection(Section);
        const SectionSymbolTable::IndexLookupType& Vector =
            SectionSymtab.getVector();
        Writer->writeVaruint32(Vector.size(), WritePos);
        for (const SymbolNode* Symbol : Vector)
          writeSymbol(Symbol);
        for (int i = 0, len = Nd->getNumKids(); i < len; ++i)
          writeNode(Nd->getKid(i));
      });
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
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint32(Nd->getNumKids(), WritePos);
      break;
    }
    case OpSymbol: {
      Writer->writeUint8(Opcode, WritePos);
      SymbolNode* Sym = cast<SymbolNode>(const_cast<Node*>(Nd));
      Writer->writeVaruint32(SectionSymtab.getSymbolIndex(Sym), WritePos);
      break;
    }
  }
}

void BinaryWriter::writeBlock(std::function<void()> ApplyFn) {
  WriteCursor BlockStart(WritePos);
  Writer->writeFixedBlockSize(WritePos, 0);
  size_t SizeAfterSizeWrite = Writer->getStreamAddress(WritePos);
  ApplyFn();
  const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
  if (!MinimizeBlockSize) {
    Writer->writeFixedBlockSize(BlockStart, NewSize);
  } else {
    Writer->writeVarintBlockSize(BlockStart, NewSize);
    size_t SizeAfterBackPatch = Writer->getStreamAddress(BlockStart);
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t CurAddress = Writer->getStreamAddress(WritePos);
      Writer->moveBlock(BlockStart, SizeAfterSizeWrite,
                        (CurAddress - Diff) - SizeAfterBackPatch);
      WritePos.swap(BlockStart);
    }
  }
}

void BinaryWriter::writeSymbol(const Node* Symbol) {
  TRACE_METHOD("writeSymbol");
  TRACE(node_ptr, nullptr, Symbol);
  assert(isa<SymbolNode>(Symbol) &&
         "BinaryWriter::writeSymbol called on non-symbol");
  const auto* Sym = cast<SymbolNode>(Symbol);
  const std::string& SymName = Sym->getName();
  Writer->writeVaruint32(SymName.size(), WritePos);
  for (size_t i = 0, len = SymName.size(); i < len; ++i)
    Writer->writeUint8(SymName[i], WritePos);
}

}  // end of namespace filt

}  // end of namespace wasm
