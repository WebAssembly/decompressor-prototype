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

}  // end of anonymous namespace

Interpreter::Interpreter(std::shared_ptr<Queue> Input,
                         std::shared_ptr<Queue> Output,
                         std::shared_ptr<SymbolTable> Symtab)
    : ReadPos(StreamType::Byte, Input),
      Reader(std::make_shared<ByteReadStream>()),
      WritePos(StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      Symtab(Symtab),
      LastReadValue(0),
      MinimizeBlockSize(false),
      Trace(ReadPos, WritePos, "InterpSexp") {
  DefaultFormat = Symtab->create<Varuint64NoArgsNode>();
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
  TRACE_METHOD("eval", Trace);
  Trace.traceSexp(Nd);
  IntType ReturnValue = 0;
  switch (NodeType Type = Nd->getType()) {
    case NO_SUCH_NODETYPE:
    case OpConvert:
    case OpFilter:
    case OpBlockEndNoArgs:
    case OpSymbol:
      // TODO(kschimpf): Fix above cases.
      fprintf(stderr, "Not implemented: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      break;
    case OpDefine:
    case OpFile:
    case OpSection:
    case OpUndefine:
    case OpRename:
    case OpVersion:
    case OpUnknownSection:
      fprintf(stderr, "Evaluating not allowed: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      break;
    case OpMap:
    case OpOpcode:
      ReturnValue = write(read(Nd), Nd);
      break;
    case OpLastRead:
      ReturnValue = read(Nd);
      break;
    case OpSwitch: {
      const auto* Sel = cast<SwitchNode>(Nd);
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
    case OpStream: {
      const auto* Stream = cast<StreamNode>(Nd);
      switch (Stream->getStreamKind()) {
        case StreamKind::Input:
          switch (Stream->getStreamType()) {
            case StreamType::Byte:
              ReturnValue = int(isa<ByteReadStream>(Reader.get()));
              break;
            case StreamType::Bit:
            case StreamType::Int:
            case StreamType::Ast:
              Trace.errorSexp("Stream check: ", Nd);
              fatal("Stream check not implemented");
              break;
          }
        case StreamKind::Output:
          switch (Stream->getStreamType()) {
            case StreamType::Byte:
              ReturnValue = int(isa<ByteReadStream>(Writer.get()));
              break;
            case StreamType::Bit:
            case StreamType::Int:
            case StreamType::Ast:
              Trace.errorSexp("Stream check: ", Nd);
              fatal("Stream check not implemented");
              break;
          }
      }
      break;
    }
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
    case OpU8Const:
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
    case OpWrite:
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
      return cast<Uint8OneArgNode>(Nd)->getValue();
    case OpUint32NoArgs:
      Value = read(Nd);
      return 32;
    case OpUint32OneArg:
      Value = read(Nd);
      return cast<Uint32OneArgNode>(Nd)->getValue();
    case OpUint64NoArgs:
      Value = read(Nd);
      return 64;
    case OpUint64OneArg:
      Value = read(Nd);
      return cast<Uint64OneArgNode>(Nd)->getValue();
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
  TRACE_METHOD("readOpcode", Trace);
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

      return LastReadValue = 0;
    case OpMap: {
      const auto *Map = cast<MapNode>(Nd);
      const CaseNode *Case = Map->getCase(read(Map->getKid(0)));
      return read(Case->getKid(1));
    }
    case OpOpcode: {
      return LastReadValue = readOpcode(Nd, 0, 0);
    }
    case OpI32Const:
    case OpI64Const:
    case OpU8Const:
    case OpU32Const:
    case OpU64Const: {
      return LastReadValue = dyn_cast<IntegerNode>(Nd)->getValue();
    }
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
      return LastReadValue = Reader->readUint8Bits(
                 ReadPos, cast<Uint8OneArgNode>(Nd)->getValue());
    case OpUint32NoArgs:
      return LastReadValue = Reader->readUint32(ReadPos);
    case OpUint32OneArg:
      return LastReadValue = Reader->readUint32Bits(
                 ReadPos, cast<Uint32OneArgNode>(Nd)->getValue());
    case OpUint64NoArgs:
      return LastReadValue = Reader->readUint64(ReadPos);
    case OpUint64OneArg:
      return LastReadValue = Reader->readUint64Bits(
                 ReadPos, cast<Uint64OneArgNode>(Nd)->getValue());
    case OpVarint32NoArgs:
      return LastReadValue = Reader->readVarint32(ReadPos);
    case OpVarint32OneArg:
      return LastReadValue = Reader->readVarint32Bits(
                 ReadPos, cast<Varint32OneArgNode>(Nd)->getValue());
    case OpVarint64NoArgs:
      return LastReadValue = Reader->readVarint64(ReadPos);
    case OpVarint64OneArg:
      return LastReadValue = Reader->readVarint64Bits(
                 ReadPos, cast<Varint64OneArgNode>(Nd)->getValue());
    case OpVaruint32NoArgs:
      return LastReadValue = Reader->readVaruint32(ReadPos);
    case OpVaruint32OneArg:
      return LastReadValue = Reader->readVaruint32Bits(
                 ReadPos, cast<Varuint32OneArgNode>(Nd)->getValue());
    case OpVaruint64NoArgs:
      return LastReadValue = Reader->readVaruint64(ReadPos);
    case OpVaruint64OneArg:
      return LastReadValue = Reader->readVaruint64Bits(
                 ReadPos, cast<Varuint64OneArgNode>(Nd)->getValue());
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
    case OpUint8NoArgs:
      Writer->writeUint8(Value, WritePos);
      break;
    case OpUint8OneArg:
      Writer->writeUint8Bits(Value, WritePos,
                             cast<Uint8OneArgNode>(Nd)->getValue());
      break;
    case OpUint32NoArgs:
      Writer->writeUint32(Value, WritePos);
      break;
    case OpUint32OneArg:
      Writer->writeUint32Bits(Value, WritePos,
                              cast<Uint32OneArgNode>(Nd)->getValue());
      break;
    case OpUint64NoArgs:
      Writer->writeUint64(Value, WritePos);
      break;
    case OpUint64OneArg:
      Writer->writeUint64Bits(Value, WritePos,
                              cast<Uint64OneArgNode>(Nd)->getValue());
      break;
    case OpVarint32NoArgs:
      Writer->writeVarint32(Value, WritePos);
      break;
    case OpVarint32OneArg:
      Writer->writeVarint32Bits(Value, WritePos,
                                cast<Varint32OneArgNode>(Nd)->getValue());
      break;
    case OpVarint64NoArgs:
      Writer->writeVarint64(Value, WritePos);
      break;
    case OpVarint64OneArg:
      Writer->writeVarint64Bits(Value, WritePos,
                                cast<Varint64OneArgNode>(Nd)->getValue());
      break;
    case OpVaruint32NoArgs:
      Writer->writeVaruint32(Value, WritePos);
      break;
    case OpVaruint32OneArg:
      Writer->writeVaruint32Bits(Value, WritePos,
                                 cast<Varuint32OneArgNode>(Nd)->getValue());
      break;
    case OpVaruint64NoArgs:
      Writer->writeVaruint64(Value, WritePos);
      break;
    case OpVaruint64OneArg:
      Writer->writeVaruint64Bits(Value, WritePos,
                                 cast<Varuint64OneArgNode>(Nd)->getValue());
      break;
    case OpI32Const:
    case OpI64Const:
    case OpU8Const:
    case OpU32Const:
    case OpU64Const:
    case OpMap:
    case OpPeek:
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
  TRACE_METHOD("decompress", Trace);
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
  TRACE_METHOD("decompressBlock", Trace);
  const uint32_t OldSize = Reader->readBlockSize(ReadPos);
  Trace.traceUint32_t("block size", OldSize);
  Reader->pushEobAddress(ReadPos, OldSize);
  Cursor BlockStart(WritePos);
  Writer->writeFixedBlockSize(WritePos, 0);
  size_t SizeAfterSizeWrite = Writer->getStreamAddress(WritePos);
  evalOrCopy(Code);
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
  TRACE_METHOD("decompressSection", Trace);
  LastReadValue = 0;
  assert(isa<ByteReadStream>(Reader.get()));
#if LOG_SECTIONS
  size_t SectionAddress = ReadPos.getCurByteAddress();
#endif
  readSectionName();
#if LOG_SECTIONS
  fprintf(stderr, "@%" PRIxMAX " section '%s'\n", uintmax_t(SectionAddress),
          CurSectionName.c_str());
#endif
  Trace.traceString("name", CurSectionName);
  SymbolNode* Sym = Symtab->getSymbol(CurSectionName);
  decompressBlock(Sym ? Sym->getDefineDefinition() : nullptr);
  Reader->alignToByte(ReadPos);
  Writer->alignToByte(WritePos);
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
