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

#include "interp/Interpreter.h"
#include "sexp/TextWriter.h"

#include <iostream>

// The following turn on logging sections, functions, and bytes at each eval
// call in the decompression algorithm.
#define LOG_SECTIONS 0
#define LOG_FUNCTIONS 0
#define LOG_EVAL 0

// The following two defines allows turning on tracing for the nth (zero based)
// function.
#define LOG_NUMBERED_BLOCK 0
#define LOG_FUNCTION_NUMBER 0

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

namespace {

static constexpr uint32_t MaxExpectedSectionNameSize = 32;

IntType getIntegerValue(Node* N) {
  if (auto* IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

}  // end of anonymous namespace

Interpreter::Interpreter(ByteQueue* Input,
                         ByteQueue* Output,
                         SymbolTable* Algorithms)
    : ReadPos(StreamType::Byte, Input),
      WritePos(StreamType::Byte, Output),
      Alloc(Allocator::Default),
      Algorithms(Algorithms),
      LastReadValue(0),
      MinimizeBlockSize(false),
      Trace(ReadPos, WritePos, "InterpSexp") {
  Reader = Alloc->create<ByteReadStream>();
  Writer = Alloc->create<ByteWriteStream>();
  DefaultFormat = Alloc->create<Varuint64NoArgsNode>();
  CurSectionName.reserve(MaxExpectedSectionNameSize);
}

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

IntType Interpreter::eval(const Node* Nd) {
  // TODO(kschimpf): Fix for ast streams.
  // TODO(kschimpf) Handle blocks.
  TraceClass::Method _("eval", Trace);
  Trace.traceSexp(Nd);
  IntType ReturnValue = 0;
  switch (NodeType Type = Nd->getType()) {
    case NO_SUCH_NODETYPE:
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
    case OpOpcode:
      ReturnValue = write(read(Nd), Nd);
      break;
    case OpLastRead:
      ReturnValue = read(Nd);
      break;
    case OpSelect: {
      const SelectNode* Sel = cast<SelectNode>(Nd);
      IntType Selector = eval(Sel->getKid(0));
      if (const auto* Case = Sel->getCase(Selector))
        eval(Case);
      else
        eval(Sel->getKid(1));
      break;
    }
    case OpCase:
      eval(Nd->getKid(1));
      break;
    case OpBlock:
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
      // NOTE: This assumes that blocks (outside of sections) are only
      // used to define functions.
      fprintf(stderr, "@%" PRIxMAX "/@%" PRIxMAX " Function %" PRIuMAX "\n",
              uintmax_t(ReadPos.getCurByteAddress()),
              uintmax_t(WritePos.getCurByteAddress()),
              uintmax_t(LogBlockCount));
#if LOG_NUMBERED_BLOCK
      if (LogBlockCount == LOG_FUNCTION_NUMBER)
        Trace.setTraceProgress(true);
#endif
#endif
      decompressBlock(Nd->getKid(0));
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCKS
#if LOG_NUMBERED_BLOCK
      if (LogBlockCount == LOG_FUNCTION_NUMBER)
        Trace.setTraceProgress(0);
#endif
      ++LogBlockCount;
#endif
      break;
    case OpAnd:
      if (eval(Nd->getKid(0)) != 0 && eval(Nd->getKid(1)) != 0)
        ReturnValue = 1;
      break;
    case OpNot:
      if (eval(Nd->getKid(0)) == 0)
        ReturnValue = 1;
      break;
    case OpOr:
      if (eval(Nd->getKid(0)) != 0 || eval(Nd->getKid(1)) != 0)
        ReturnValue = 1;
      break;
    case OpIsByteIn:
      ReturnValue = int(isa<ByteReadStream>(Reader));
      break;
    case OpIsByteOut:
      ReturnValue = int(isa<ByteWriteStream>(Writer));
      break;
    case OpError:
      fatal("Error found during evaluation");
      break;
    case OpEval:
#ifdef LOG_EVAL
      if (Trace.getTraceProgress()) {
        Trace.indent();
        decode::Cursor Lookahead(ReadPos);
        for (size_t i = 0; i < 10; ++i) {
          if (!Lookahead.atEob())
            fprintf(Trace.getFile(), " %x", Lookahead.readByte());
        }
        fprintf(Trace.getFile(), "\n");
      }
#endif
      if (auto* Sym = dyn_cast<SymbolNode>(Nd->getKid(0))) {
        ReturnValue = eval(Sym->getDefineDefinition());
        break;
      }
      fatal("Can't evaluate symbol");
      break;
    case OpEvalDefault: {
      if (auto* Sym = dyn_cast<SymbolNode>(Nd->getKid(0))) {
        ReturnValue = eval(Sym->getDefaultDefinition());
        break;
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
      ReturnValue = read(Nd);
      break;
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
      while (!ReadPos.atEob())
        for (auto* Kid : *Nd)
          eval(Kid);
      break;
    }
    case OpMap:
      ReturnValue = write(read(Nd->getKid(0)), Nd->getKid(1));
      break;
    case OpPeek:
      ReturnValue = read(Nd);
      break;
    case OpRead:
      ReturnValue = read(Nd->getKid(1));
      break;
    case OpSequence:
      for (auto* Kid : *Nd)
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
      ReturnValue = write(read(Nd), Nd);
      break;
    case OpVoid:
      break;
  }
  Trace.traceIntType("return value", ReturnValue);
  return ReturnValue;
}

uint32_t Interpreter::readOpcodeSelector(const Node* Nd, IntType& Value) {
  switch (Nd->getType()) {
    case OpUint8NoArgs:
      Value = read(Nd);
      return 8;
    case OpUint8OneArg:
      Value = read(Nd);
      return isa<ByteReadStream>(Reader) ? getIntegerValue(Nd->getKid(0)) : 8;
    case OpUint32NoArgs:
      Value = read(Nd);
      return 32;
    case OpUint32OneArg:
      Value = read(Nd);
      return isa<ByteReadStream>(Reader) ? getIntegerValue(Nd->getKid(0)) : 32;
    case OpUint64NoArgs:
      Value = read(Nd);
      return 64;
    case OpUint64OneArg:
      Value = read(Nd);
      return isa<ByteReadStream>(Reader) ? getIntegerValue(Nd->getKid(0)) : 64;
    case OpEval:
      if (auto* Sym = dyn_cast<SymbolNode>(Nd->getKid(0)))
        return readOpcodeSelector(Sym->getDefineDefinition(), Value);
      fatal("Can't evaluate symbol");
      return 0;
    default:
      Value = read(Nd);
      return 0;
  }
}

IntType Interpreter::readOpcode(const Node* Nd,
                                IntType PrefixValue,
                                uint32_t NumOpcodes) {
  TraceClass::Method _("readOpcode", Trace);
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Illegal opcode selector: %s\n", getNodeTypeName(Type));
      fatal("Unable to read opcode");
      break;
    case OpOpcode: {
      const auto* Sel = cast<OpcodeNode>(Nd);
      const Node* SelectorNd = Sel->getKid(0);
      uint32_t SelectorSize = readOpcodeSelector(SelectorNd, LastReadValue);
      Trace.traceUint32_t("selector value", LastReadValue);
      if (NumOpcodes > 0) {
        Trace.traceIntType("prefix value", PrefixValue);
        Trace.traceUint32_t("selector bitsize", SelectorSize);
        if (SelectorSize < 1 || SelectorSize >= 64)
          fatal("Opcode selector has illegal bitsize");
        LastReadValue |= PrefixValue << SelectorSize;
      }
      if (const CaseNode* Case = Sel->getCase(LastReadValue)) {
        LastReadValue = eval(Case);
      }
      break;
    }
  }
  Trace.traceIntType("return value", LastReadValue);
  return LastReadValue;
}

IntType Interpreter::read(const Node* Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Read not implemented: %s\n", getNodeTypeName(Type));
      fatal("Read not implemented");

      return 0;
    case OpOpcode: {
      return LastReadValue = readOpcode(Nd, 0, 0);
    }
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
    case OpPeek: {
      Cursor InitialPos(ReadPos);
      LastReadValue = read(Nd->getKid(0));
      ReadPos.swap(InitialPos);
      return LastReadValue;
    }
    case OpLastRead:
      return LastReadValue;
    case OpUint8NoArgs:
      return LastReadValue = Reader->readUint8(ReadPos);
    case OpUint8OneArg:
      return LastReadValue =
                 Reader->readUint8Bits(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint32NoArgs:
      return LastReadValue = Reader->readUint32(ReadPos);
    case OpUint32OneArg:
      return LastReadValue = Reader->readUint32Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint64NoArgs:
      return LastReadValue = Reader->readUint64(ReadPos);
    case OpUint64OneArg:
      return LastReadValue = Reader->readUint64Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint32NoArgs:
      return LastReadValue = Reader->readVarint32(ReadPos);
    case OpVarint32OneArg:
      return LastReadValue = Reader->readVarint32Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint64NoArgs:
      return LastReadValue = Reader->readVarint64(ReadPos);
    case OpVarint64OneArg:
      return LastReadValue = Reader->readVarint64Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint32NoArgs:
      return LastReadValue = Reader->readVaruint32(ReadPos);
    case OpVaruint32OneArg:
      return LastReadValue = Reader->readVaruint32Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint64NoArgs:
      return LastReadValue = Reader->readVaruint64(ReadPos);
    case OpVaruint64OneArg:
      return LastReadValue = Reader->readVaruint64Bits(
                 ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVoid:
      return LastReadValue = 0;
  }
}

IntType Interpreter::write(IntType Value, const wasm::filt::Node* Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Write not implemented: %s\n", getNodeTypeName(Type));
      fatal("Write not implemented");
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
    case OpOpcode: {
      const auto* Sel = cast<OpcodeNode>(Nd);
      uint32_t SelShift;
      IntType CaseMask;
      const CaseNode* Case = Sel->getWriteCase(Value, SelShift, CaseMask);
      write(Value >> SelShift, Sel->getKid(0));
      if (Case)
        write(Value & CaseMask, Case->getKid(1));
      break;
    }
  }
  return Value;
}

void Interpreter::decompress() {
  TraceClass::Method _("decompress", Trace);
  LastReadValue = 0;
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  Trace.traceUint32_t("magic number", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to decompress, did not find WASM binary magic number");
  Writer->writeUint32(MagicNumber, WritePos);
  Version = Reader->readUint32(ReadPos);
  Trace.traceHexUint32_t("version", Version);
  if (Version != WasmBinaryVersion)
    fatal("Unable to decompress, WASM version number not known");
  Writer->writeUint32(Version, WritePos);

  while (!ReadPos.atEob())
    decompressSection();
  WritePos.freezeEof();
}

void Interpreter::decompressBlock(const Node* Code) {
  TraceClass::Method _("decompressBlock", Trace);
  auto* ByteWriter = dyn_cast<ByteWriteStream>(Writer);
  bool IsByteReader = isa<ByteReadStream>(Reader);
  if (IsByteReader) {
    const uint32_t BlockSize = Reader->readVaruint32(ReadPos);
    Trace.traceUint32_t("block size", BlockSize);
    ReadPos.pushEobAddress(ReadPos.getCurByteAddress() + BlockSize);
  }
  if (ByteWriter) {
    Cursor BlockPos(WritePos);
    ByteWriter->writeFixedVaruint32(0, WritePos);
    size_t SizeAfterSizeWrite = WritePos.getCurByteAddress();
    evalOrCopy(Code);
    const size_t NewSize =
        WritePos.getCurByteAddress() -
        (BlockPos.getCurByteAddress() + ByteWriteStream::ChunksInWord);
    if (!MinimizeBlockSize) {
      ByteWriter->writeFixedVaruint32(NewSize, BlockPos);
    } else {
      ByteWriter->writeVaruint32(NewSize, BlockPos);
      size_t SizeAfterBackPatch = BlockPos.getCurByteAddress();
      size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
      if (Diff) {
        size_t End = WritePos.getCurByteAddress() - Diff;
        Cursor CopyPos(StreamType::Byte, WritePos.getQueue());
        CopyPos.jumpToByteAddress(SizeAfterSizeWrite);
        for (size_t i = SizeAfterBackPatch; i < End; ++i)
          BlockPos.writeByte(CopyPos.readByte());
        WritePos.jumpToByteAddress(BlockPos.getCurByteAddress());
      }
    }
  } else {
    evalOrCopy(Code);
  }
  if (IsByteReader)
    ReadPos.popEobAddress();
}

void Interpreter::evalOrCopy(const Node* Nd) {
  if (Nd) {
    eval(Nd);
    return;
  }
  while (!ReadPos.atEob())
    Writer->writeUint8(Reader->readUint8(ReadPos), WritePos);
}

void Interpreter::decompressSection() {
  // TODO(kschimpf) Handle 'filter' sections specially (i.e. install).  This
  // includes calling "clearCaches" on all filter s-expressions to remove an
  // (optimizing) caches installed.
  TraceClass::Method _("decompressSection", Trace);
  LastReadValue = 0;
  assert(isa<ByteReadStream>(Reader));
#if LOG_SECTIONS
  size_t SectionAddress = ReadPos.getCurByteAddress();
#endif
  readSectionName();
#if LOG_SECTIONS
  fprintf(stderr, "@%" PRIxMAX " section '%s'\n", uintmax_t(SectionAddress),
          sectionName);
#endif
  Trace.traceString("name", CurSectionName);
  SymbolNode* Sym = Algorithms->getSymbol(CurSectionName);
  decompressBlock(Sym ? Sym->getDefineDefinition() : nullptr);
}

void Interpreter::readSectionName() {
  CurSectionName.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  Writer->writeVaruint32(NameSize, WritePos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Writer->writeUint8(Byte, WritePos);
    CurSectionName.push_back(char(Byte));
  }
}

}  // end of namespace interp.

}  // end of namespace wasm.
