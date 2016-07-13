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
#include "binary/BinGen.h"
#include "interp/State.h"

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace interp;

namespace filt {

namespace {

IntType getIntegerValue(Node *N) {
  if (auto *IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

} // end of anonymous namespace

BinGen::BinGen(decode::ByteQueue *Output) :
    WritePos(Output), Alloc(Allocator::Default) {
  Writer = Alloc->create<ByteWriteStream>();
}

BinGen::~BinGen() {
  WritePos.freezeEob();
  delete TraceWriter;
}

void BinGen::setTraceProgress(bool NewValue) {
  TraceProgress = NewValue;
}

TextWriter *BinGen::getTraceWriter() {
  if (TraceWriter == nullptr)
    TraceWriter = new TextWriter();
  return TraceWriter;
}

void BinGen::writeIndent() {
  for (int i = 0; i < IndentLevel; ++i)
    fprintf(stderr, "  ");
  fprintf(stderr, "@%" PRIuMAX " ", intmax_t(WritePos.getCurAddress()));
}

template<class Type>
Type BinGen::returnValue(const char *Name, Type Value) {
  if (!TraceProgress)
    return Value;
  IndentEnd();
  fprintf(stderr, "<- %s = %d\n", Name, int(Value));
  return Value;
}

void BinGen::enter(const char *Name, bool AddNewline) {
  IndentBegin();
  fprintf(stderr, "-> %s", Name);
  if (AddNewline)
    fputc('\n', stderr);
  else
    fputc(' ', stderr);
}

void BinGen::exit(const char *Name) {
  IndentEnd();
  fprintf(stderr, "<- %s\n", Name);
}

void BinGen::writePreamble() {
  Writer->writeUint32(WasmBinaryMagic, WritePos);
  Writer->writeUint32(WasmBinaryVersion, WritePos);
}

void BinGen::writeFile(const FileNode *File) {
  if (TraceProgress) {
    enter("writeFile");
  }
  writeNode(File);
  exit("writeFile");
}

void BinGen::writeSection(const SectionNode *Section) {
  if (TraceProgress) {
    enter("writeSection", false);
    getTraceWriter()->writeAbbrev(stderr, Section);
  }
  writeNode(Section);
  exit("writeSection");
}

void BinGen::writeNode(const Node *Nd) {
  if (TraceProgress) {
    enter("writeNode", false);
    getTraceWriter()->writeAbbrev(stderr, Nd);
  }
  switch (NodeType Type = Nd->getType()) {
    case OpInteger: {
      // TODO(kschimpf) Fix this list.
      fprintf(stderr, "Misplaced s-expression: %s\n", getNodeTypeName(Type));
      fatal("Unable to write filter s-expression");
      break;
    }
    case OpAnd:
    case OpBlock:
    case OpByteToByte:
    case OpOr:
    case OpNot:
    case OpBlockEndNoArgs:
    case OpDefault:
    case OpDefine:
    case OpError:
    case OpEval:
    case OpEvalDefault:
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
    case OpVoid: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto *Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Type, WritePos);
      break;
    }
    case OpCase: {
      writeNode(Nd->getKid(1));
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpFile: {
      for (const auto *Kid : *Nd)
        writeNode(Kid);
      break;
    }
    case OpSection: {
      writeNode(Nd->getKid(0)); // name of section
      writeBlock(
          [&]() {
            for (int i = 1, len = Nd->getNumKids(); i < len; ++i)
              writeNode(Nd->getKid(i));
          });
      break;
    }
    case OpFilter:
    case OpSelect:
    case OpSequence: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto *Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint32(Nd->getNumKids(), WritePos);
      break;
    }
    case OpSymbol: {
      const SymbolNode *Sym = cast<SymbolNode>(Nd);
      InternalName SymName = Sym->getName();
      Writer->writeVaruint32(SymName.size(), WritePos);
      for (size_t i = 0, len = SymName.size(); i < len; ++i)
        Writer->writeUint8(SymName[i], WritePos);
      break;
    }
    case OpUint32OneArg:
    case OpUint64OneArg:
    case OpUint8OneArg:
    case OpU32Const:
    case OpU64Const:
    case OpVarint32OneArg:
    case OpVarint64OneArg:
    case OpVaruint32OneArg:
    case OpVaruint64OneArg:
    case OpVersion: {
      // Operations that get an unsigned integer argument.
      Writer->writeUint8(Type, WritePos);
      Writer->writeVaruint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
    case OpI32Const:
    case OpI64Const: {
      // Operations that get a signed integer argument.
      Writer->writeUint8(Type, WritePos);
      Writer->writeVarint32(getIntegerValue(Nd->getKid(0)), WritePos);
      break;
    }
  }
  exit("writeNode");
}

void BinGen::writeBlock(std::function<void()> ApplyFn) {
  WriteCursor BlockPos(WritePos);
  Writer->writeFixedVaruint32(0, WritePos);
  size_t SizeAfterSizeWrite = WritePos.getCurAddress();
  ApplyFn();
  const size_t NewSize =
      WritePos.getCurAddress() - (BlockPos.getCurAddress() +
                                  ByteWriteStream::ChunksInWord);
  if (!MinimizeBlockSize) {
    Writer->writeFixedVaruint32(NewSize, BlockPos);
  } else {
    Writer->writeVaruint32(NewSize, BlockPos);
    size_t SizeAfterBackPatch = BlockPos.getCurAddress();
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t End = WritePos.getCurAddress() - Diff;
      ReadCursor CopyPos(WritePos.getQueue());
      CopyPos.jumpToAddress(SizeAfterSizeWrite);
      for (size_t i = SizeAfterBackPatch; i < End; ++ i)
        BlockPos.writeByte(CopyPos.readByte());
      WritePos.jumpToAddress(BlockPos.getCurAddress());
    }
  }
}

} // end of namespace filt

} // end of namespace wasm
