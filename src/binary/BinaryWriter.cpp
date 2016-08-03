/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Implements a binary generator for filter s-expressions.

#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "binary/BinaryWriter.h"

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

namespace {

IntType getIntegerValue(Node* N) {
  if (auto* IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

}  // end of anonymous namespace

BinaryWriter::BinaryWriter(decode::ByteQueue* Output, SymbolTable& Symtab)
    : WritePos(decode::StreamType::Byte, Output),
      Writer(nullptr),
      SectionSymtab(Symtab),
      MinimizeBlockSize(false),
      Trace(WritePos, "BinaryWriter") {
  Writer = Symtab.getAllocator()->create<ByteWriteStream>();
}

void BinaryWriter::writePreamble() {
  Writer->writeUint32(WasmBinaryMagic, WritePos);
  Writer->writeUint32(WasmBinaryVersion, WritePos);
}

void BinaryWriter::writeFile(const FileNode* File) {
  TraceClass::Method _("writeFile", Trace);
  Trace.traceSexp(File);
  writeNode(File);
}

void BinaryWriter::writeSection(const SectionNode* Section) {
  TraceClass::Method _("writeSection", Trace);
  Trace.traceSexp(Section);
  writeNode(Section);
}

void BinaryWriter::writeNode(const Node* Nd) {
  TraceClass::Method _("writeNode", Trace);
  Trace.traceSexp(Nd);
  switch (NodeType Type = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpUnknownSection:
    case OpInteger: {
      // TODO(kschimpf) Fix this list.
      fprintf(stderr, "Misplaced s-expression: %s\n", getNodeTypeName(Type));
      fatal("Unable to write filter s-expression");
      break;
    }
    case OpAnd:
    case OpBlock:
    case OpBlockEndNoArgs:
    case OpByteToByte:
    case OpOr:
    case OpNot:
    case OpError:
    case OpIfThen:
    case OpIfThenElse:
    case OpIsByteIn:
    case OpIsByteOut:
    case OpLoop:
    case OpLoopUnbounded:
    case OpMap:
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
    case OpVoid: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Type, WritePos);
      break;
    }
    case OpCase: {
      writeNode(Nd->getKid(1));
      Writer->writeUint8(Type, WritePos);
      Writer->writeVarint64(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpFile: {
      for (const auto* Kid : *Nd)
        writeNode(Kid);
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
    case OpSelect:
    case OpSequence: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Type, WritePos);
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
      Writer->writeUint8(Type, WritePos);
      Writer->writeUint8(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpU32Const: {
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpU64Const: {
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint64(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpVersion: {
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpI32Const: {
      Writer->writeUint8(Type, WritePos);
      Writer->writeVarint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpI64Const: {
      Writer->writeUint8(Type, WritePos);
      Writer->writeVarint64(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpEval:
    case OpEvalDefault: {
      Writer->writeUint8(Type, WritePos);
      writeNode(Nd->getKid(0));
      break;
    }
    case OpDefault:
    case OpDefine: {
      writeNode(Nd->getKid(1));
      Writer->writeUint8(Type, WritePos);
      writeNode(Nd->getKid(0));
    }
  }
}

void BinaryWriter::writeBlock(std::function<void()> ApplyFn) {
  Cursor BlockPos(WritePos);
  Writer->writeFixedVaruint32(0, WritePos);
  size_t SizeAfterSizeWrite = WritePos.getCurByteAddress();
  ApplyFn();
  const size_t NewSize =
      WritePos.getCurByteAddress() -
      (BlockPos.getCurByteAddress() + ByteWriteStream::ChunksInWord);
  if (!MinimizeBlockSize) {
    Writer->writeFixedVaruint32(NewSize, BlockPos);
  } else {
    Writer->writeVaruint32(NewSize, BlockPos);
    size_t SizeAfterBackPatch = BlockPos.getCurByteAddress();
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t End = WritePos.getCurByteAddress() - Diff;
      Cursor CopyPos(WritePos.getType(), WritePos.getQueue());
      CopyPos.jumpToByteAddress(SizeAfterSizeWrite);
      for (size_t i = SizeAfterBackPatch; i < End; ++i)
        BlockPos.writeByte(CopyPos.readByte());
      WritePos.jumpToByteAddress(BlockPos.getCurByteAddress());
    }
  }
}

void BinaryWriter::writeSymbol(const Node* Symbol) {
  TraceClass::Method _("writeSymbol", Trace);
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
