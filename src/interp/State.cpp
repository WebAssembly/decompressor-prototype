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

// Implements the interpreter for filter s-expressions.

#include "interp/State.h"
#include "sexp/TextWriter.h"

#include <iostream>

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;

namespace interp {

namespace {

static constexpr uint32_t MaxExpectedSectionNameSize = 32;

IntType getIntegerValue(Node *N) {
  if (auto *IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

} // end of anonymous namespace

State::State(ByteQueue *Input, ByteQueue *Output,
             SymbolTable *Algorithms) :
    ReadPos(Input), WritePos(Output), Alloc(Allocator::Default),
    Algorithms(Algorithms) {
  Reader = Alloc->create<ByteReadStream>();
  Writer = Alloc->create<ByteWriteStream>();
  DefaultFormat = Alloc->create<Varuint64NoArgsNode>();
  CurSectionName.reserve(MaxExpectedSectionNameSize);
}

State::~State() {
  delete TraceWriter;
}

void State::setTraceProgress(bool NewValue) {
  TraceProgress = NewValue;
  if (TraceProgress && TraceWriter == nullptr)
    TraceWriter = new TextWriter();
}

void State::writeIndent() {
  for (int i = 0; i < IndentLevel; ++i)
    fprintf(stderr, "  ");
}

void State::traceStreamLocs() {
  fprintf(stderr, "@%" PRIuMAX "/@%" PRIuMAX, intmax_t(ReadPos.getCurAddress()),
          intmax_t(WritePos.getCurAddress()));
}

void State::enter(const char *Name, bool AddNewline) {
  IndentBegin();
  traceStreamLocs();
  fprintf(stderr, " -> %s", Name);
  if (AddNewline)
    fputc('\n', stderr);
}

void State::exit(const char *Name) {
  IndentEnd();
  traceStreamLocs();
  fprintf(stderr, " <- %s\n", Name);
}

IntType State::returnValueInternal(const char *Name, IntType Value) {
  IndentEnd();
  traceStreamLocs();
  fprintf(stderr, " <- %s = %" PRI_IntType "\n", Name, Value);
  return Value;
}

IntType State::eval(const Node *Nd) {
  // TODO(kschimpf): Fix for ast streams.
  // TODO(kschimpf) Handle blocks.
  if (TraceProgress) {
    enter("eval ", false);
    TraceWriter->writeAbbrev(stderr, Nd);
  }
  switch (NodeType Type = Nd->getType()) {
    case OpByteToByte:
    case OpFilter:
    case OpBlockEndNoArgs:
    case OpSymbol:
      // TODO(kschimpf): Fix above cases.
      fprintf(stderr, "Not implemented: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      break;
    case OpDefault:
    case OpDefine:
    case OpFile:
    case OpSection:
    case OpUndefine:
    case OpVersion:
    case OpInteger:
    case OpUnknownSection:
      fprintf(stderr, "Evaluating not allowed: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      break;
    case OpSelect: {
      const SelectNode *Sel = cast<SelectNode>(Nd);
      IntType Selector = eval(Sel->getKid(0));
      if (const auto *Case = Sel->getCase(Selector))
        eval(Case);
      else
        eval(Sel->getKid(1));
      break;
    }
    case OpCase:
      eval(Nd->getKid(1));
      break;
    case OpBlock:
      decompressBlock(Nd->getKid(0));
      break;
    case OpAnd:
      if (eval(Nd->getKid(0)) != 0 && eval(Nd->getKid(1)) != 0)
        return returnValue("eval", 1);
      break;
    case OpNot:
      if (eval(Nd->getKid(0)) == 0)
        return returnValue("eval", 1);
      break;
    case OpOr:
      if (eval(Nd->getKid(0)) != 0 || eval(Nd->getKid(1)) != 0)
        return returnValue("eval", 1);
      break;
    case OpIsByteIn:
      return returnValue("eval", isa<ByteReadStream>(Reader));
    case OpIsByteOut:
      return returnValue("eval", isa<ByteWriteStream>(Writer));
    case OpError:
      fatal("Error found during evaluation");
      break;
    case OpEval: {
      if (auto *Sym = dyn_cast<SymbolNode>(Nd->getKid(0))) {
        return returnValue("eval", eval(Sym->getDefineDefinition()));
      }
      fatal("Can't evaluate symbol");
      break;
    }
    case OpEvalDefault: {
      if (auto *Sym = dyn_cast<SymbolNode>(Nd->getKid(0))) {
        return returnValue("eval", eval(Sym->getDefaultDefinition()));
      }
      fatal("Can't evaluate symbol");
      break;
    }
    case OpIfThen:
      if (eval(Nd->getKid(0)) != 0)
        eval(Nd->getKid(1));
      break;
    case OpIfThenElse:
      if (eval(Nd->getKid(0)) != 0)
        eval(Nd->getKid(1));
      else
        eval(Nd->getKid(2));
      break;
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
      return returnValue("eval", read(Nd));
    case OpLoop: {
      IntType Count = eval(Nd->getKid(0));
      int NumKids = Nd->getNumKids();
      for (IntType i = 0; i < Count; ++i) {
        for (int j = 1; j < NumKids; ++j) {
          eval(Nd->getKid(j));
        }
      }
      break;
    }
    case OpLoopUnbounded: {
      while (! ReadPos.atEob())
        for (auto *Kid : *Nd)
          eval(Kid);
      break;
    }
    case OpMap:
      return returnValue("eval", write(read(Nd->getKid(0)), Nd->getKid(1)));
    case OpPeek:
      return returnValue("eval", read(Nd));
    case OpRead:
      return returnValue("eval", read(Nd->getKid(1)));
    case OpSequence:
      for (auto *Kid : *Nd)
        eval(Kid);
      break;
    case OpUint8NoArgs:
    case OpUint8OneArg:
    case OpUint32NoArgs:
    case OpUint32OneArg:
    case OpUint64NoArgs:
    case OpUint64OneArg:
    case OpVarint32NoArgs:
    case OpVarint32OneArg:
    case OpVarint64NoArgs:
    case OpVarint64OneArg:
    case OpVaruint32NoArgs:
    case OpVaruint32OneArg:
    case OpVaruint64NoArgs:
    case OpVaruint64OneArg:
      return returnValue("eval", write(read(Nd), Nd));
    case OpVoid:
      break;
  }
  return returnValue("eval", 0);
}

IntType State::read(const Node *Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Read not implemented: %s\n", getNodeTypeName(Type));
      fatal("Read not implemented");
      return 0;
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
    case OpPeek: {
      ReadCursor InitialPos(ReadPos);
      IntType Value = read(Nd->getKid(0));
      ReadPos.swap(InitialPos);
      return Value;
    }
    case OpUint8NoArgs:
      return Reader->readUint8(ReadPos);
    case OpUint8OneArg:
      return Reader->readUint8Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint32NoArgs:
      return Reader->readUint32(ReadPos);
    case OpUint32OneArg:
      return Reader->readUint32Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint64NoArgs:
      return Reader->readUint64(ReadPos);
    case OpUint64OneArg:
      return Reader->readUint64Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint32NoArgs:
      return Reader->readVarint32(ReadPos);
    case OpVarint32OneArg:
      return Reader->readVarint32Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint64NoArgs:
      return Reader->readVarint64(ReadPos);
    case OpVarint64OneArg:
      return Reader->readVarint64Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint32NoArgs:
      return Reader->readVaruint32(ReadPos);
    case OpVaruint32OneArg:
      return Reader->readVaruint32Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint64NoArgs:
      return Reader->readVaruint64(ReadPos);
    case OpVaruint64OneArg:
      return Reader->readVaruint64Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVoid:
      return 0;
  }
}

IntType State::write(IntType Value, const wasm::filt::Node *Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Read not implemented: %s\n", getNodeTypeName(Type));
      fatal("Read not implemented");
      break;
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
    case OpPeek:
    case OpUint8NoArgs:
      Writer->writeUint8(Value, WritePos);
      break;
    case OpUint8OneArg:
      Writer->writeUint8Bits(Value, WritePos, getIntegerValue(Nd->getKid(0)));
      break;
    case OpUint32NoArgs:
      Writer->writeUint32(Value, WritePos);
      break;
    case OpUint32OneArg:
      Writer->writeUint32Bits(Value, WritePos, getIntegerValue(Nd->getKid(0)));
      break;
    case OpUint64NoArgs:
      Writer->writeUint64(Value, WritePos);
      break;
    case OpUint64OneArg:
      Writer->writeUint64Bits(Value, WritePos, getIntegerValue(Nd->getKid(0)));
      break;
    case OpVarint32NoArgs:
      Writer->writeVarint32(Value, WritePos);
      break;
    case OpVarint32OneArg:
      Writer->writeVarint32Bits(Value, WritePos,
                                getIntegerValue(Nd->getKid(0)));
      break;
    case OpVarint64NoArgs:
      Writer->writeVarint64(Value, WritePos);
      break;
    case OpVarint64OneArg:
      Writer->writeVarint64Bits(Value, WritePos,
                                getIntegerValue(Nd->getKid(0)));
      break;
    case OpVaruint32NoArgs:
      Writer->writeVaruint32(Value, WritePos);
      break;
    case OpVaruint32OneArg:
      Writer->writeVaruint32Bits(Value, WritePos,
                                 getIntegerValue(Nd->getKid(0)));
      break;
    case OpVaruint64NoArgs:
      Writer->writeVaruint64(Value, WritePos);
      break;
    case OpVaruint64OneArg:
      Writer->writeVaruint64Bits(Value, WritePos,
                                 getIntegerValue(Nd->getKid(0)));
      break;
    case OpVoid:
      break;
  }
  return Value;
}

void State::decompress() {
  if (TraceProgress)
    enter("decompress");
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  if (TraceProgress) {
    writeIndent();
    traceStreamLocs();
    fprintf(stderr, " MagicNumber = %x\n", MagicNumber);
  }
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to decompress, did not find WASM binary magic number");
  Writer->writeUint32(MagicNumber, WritePos);
  Version = Reader->readUint32(ReadPos);
  if (TraceProgress) {
    writeIndent();
    traceStreamLocs();
    fprintf(stderr, " Version = %x\n", Version);
  }
  if (Version != WasmBinaryVersion)
    fatal("Unable to decompress, WASM version number not known");
  Writer->writeUint32(Version, WritePos);

  while (!ReadPos.atEob()) {
    decompressSection();
  }
  WritePos.freezeEob();
  if (TraceProgress)
    exit("decompress");
}

void State::decompressBlock(const Node *Code) {
  auto *ByteWriter = dyn_cast<ByteWriteStream>(Writer);
  bool IsByteReader = isa<ByteReadStream>(Reader);
  if (IsByteReader) {
    const size_t BlockSize = Reader->readVaruint32(ReadPos);
    if (TraceProgress) {
      writeIndent();
      traceStreamLocs();
      fprintf(stderr, " block size: %" PRIuMAX "\n", intmax_t(BlockSize));
    }
    ReadPos.pushEobAddress(ReadPos.getCurAddress() + BlockSize);
  }
  if (ByteWriter) {
    WriteCursor BlockPos(WritePos);
    ByteWriter->writeFixedVaruint32(0, WritePos);
    size_t SizeAfterSizeWrite = WritePos.getCurAddress();
    evalOrCopy(Code);
    const size_t NewSize =
        WritePos.getCurAddress() - (BlockPos.getCurAddress() +
                                    ByteWriteStream::ChunksInWord);
    if (!MinimizeBlockSize) {
      ByteWriter->writeFixedVaruint32(NewSize, BlockPos);
    } else {
      ByteWriter->writeVaruint32(NewSize, BlockPos);
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
  } else {
    evalOrCopy(Code);
  }
  if (IsByteReader)
    ReadPos.popEobAddress();
}

void State::evalOrCopy(const Node *Nd) {
  if (Nd) {
    eval(Nd);
    return;
  }
  while (!ReadPos.atEob())
    Writer->writeUint8(Reader->readUint8(ReadPos), WritePos);
}

void State::decompressSection() {
  assert(isa<ByteReadStream>(Reader));
  if (TraceProgress)
    enter("section");
  readSectionName();
  if (TraceProgress) {
    writeIndent();
    traceStreamLocs();
    fprintf(stderr, " section: '%s'\n", CurSectionName.c_str());
  }
  SymbolNode *Sym = Algorithms->getSymbol(CurSectionName);
  decompressBlock(Sym ? Sym->getDefineDefinition() : nullptr);
  if (TraceProgress)
    exit("section");
}

void State::readSectionName() {
  CurSectionName.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  Writer->writeVaruint32(NameSize, WritePos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Writer->writeUint8(Byte, WritePos);
    CurSectionName.push_back(char(Byte));
  }
}

} // end of namespace interp.

} // end of namespace wasm.
