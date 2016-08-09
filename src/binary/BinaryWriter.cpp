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

using namespace alloc;
using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

namespace {}  // end of anonymous namespace

BinaryWriter::BinaryWriter(std::shared_ptr<decode::Queue> Output,
                           std::shared_ptr<SymbolTable> Symtab)
    : WritePos(decode::StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      SectionSymtab(Symtab),
      MinimizeBlockSize(false),
      Trace(WritePos, "BinaryWriter") {
}

void BinaryWriter::writePreamble() {
  Writer->writeUint32(WasmBinaryMagic, WritePos);
  Writer->writeUint32(WasmBinaryVersion, WritePos);
}

void BinaryWriter::writeFile(const FileNode* File) {
  TRACE_METHOD("writeFile", Trace);
  Trace.traceSexp(File);
  writeNode(File);
}

void BinaryWriter::writeSection(const SectionNode* Section) {
  TRACE_METHOD("writeSection", Trace);
  Trace.traceSexp(Section);
  writeNode(Section);
}

void BinaryWriter::writeNode(const Node* Nd) {
  TRACE_METHOD("writeNode", Trace);
  Trace.traceSexp(Nd);

  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpUnknownSection: {
      // TODO(kschimpf) Fix this list.
      fprintf(stderr, "Misplaced s-expression: %s\n", getNodeTypeName(Opcode));
      fatal("Unable to write filter s-expression");
      break;
    }
    case OpAnd:
    case OpBlock:
    case OpBlockEndNoArgs:
    case OpCase:
    case OpConvert:
    case OpOr:
    case OpNot:
    case OpError:
    case OpIfThen:
    case OpIfThenElse:
    case OpLoop:
    case OpLoopUnbounded:
    case OpPeek:
    case OpRead:
    case OpUint32NoArgs:
    case OpUint64NoArgs:
    case OpUint8NoArgs:
    case OpUndefine:
    case OpVarint32NoArgs:
    case OpVarint64NoArgs:
    case OpVaruint32NoArgs:
    case OpVaruint64NoArgs:
    case OpLastRead:
    case OpRename:
    case OpVoid:
    case OpWrite: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Opcode, WritePos);
      break;
    }
    case OpFile: {
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      break;
    }
    case OpStream: {
      const auto* Stream = cast<StreamNode>(Nd);
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeUint8(Stream->getEncoding(), WritePos);
      break;
    }
    case OpSection: {
      writeSymbol(Nd->getKid(0));  // name of section
      writeBlock([&]() {
        const auto* Section = cast<SectionNode>(Nd);
        SectionSymtab.installSection(Section);
        const SectionSymbolTable::IndexLookupType& Vector =
            SectionSymtab.getVector();
        Writer->writeVaruint32(Vector.size(), WritePos);
        for (const SymbolNode* Symbol : Vector)
          writeSymbol(Symbol);
        for (int i = 1, len = Nd->getNumKids(); i < len; ++i)
          writeNode(Nd->getKid(i));
      });
      SectionSymtab.clear();
      break;
    }
    case OpFilter:
    case OpOpcode:
    case OpMap:
    case OpSwitch:
    case OpSequence: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint32(Nd->getNumKids(), WritePos);
      break;
    }
    case OpSymbol: {
      SymbolNode* Sym = cast<SymbolNode>(const_cast<Node*>(Nd));
      Writer->writeVaruint32(SectionSymtab.getSymbolIndex(Sym), WritePos);
      break;
    }
    case OpUint32OneArg:
    case OpUint64OneArg:
    case OpUint8OneArg:
    case OpVarint32OneArg:
    case OpVarint64OneArg:
    case OpVaruint32OneArg:
    case OpVaruint64OneArg: {
      // Operations that get a value in [1, 64]
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeUint8(dyn_cast<IntegerNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpU8Const: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeUint8(cast<U8ConstNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpU32Const: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint32(cast<U32ConstNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpU64Const: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint64(cast<U64ConstNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpVersion: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint32(cast<VersionNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpI32Const: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVarint32(cast<I32ConstNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpI64Const: {
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVarint64(cast<I64ConstNode>(Nd)->getValue(), WritePos);
      break;
    }
    case OpEval: {
      Writer->writeUint8(Opcode, WritePos);
      writeNode(Nd->getKid(0));
      break;
    }
    case OpDefine: {
      writeNode(Nd->getKid(1));
      Writer->writeUint8(Opcode, WritePos);
      writeNode(Nd->getKid(0));
    }
  }
}

void BinaryWriter::writeBlock(std::function<void()> ApplyFn) {
  Cursor BlockStart(WritePos);
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
  TRACE_METHOD("writeSymbol", Trace);
  Trace.traceSexp(Symbol);
  assert(isa<SymbolNode>(Symbol) &&
         "BinaryWriter::writeSymbol called on non-symbol");
  const auto* Sym = cast<SymbolNode>(Symbol);
  InternalName SymName = Sym->getName();
  Writer->writeVaruint32(SymName.size(), WritePos);
  for (size_t i = 0, len = SymName.size(); i < len; ++i)
    Writer->writeUint8(SymName[i], WritePos);
}

}  // end of namespace filt

}  // end of namespace wasm
