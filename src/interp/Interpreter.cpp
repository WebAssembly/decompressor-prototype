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

#include "interp/ByteReadStream.h"
#include "interp/ByteWriteStream.h"
#include "interp/Interpreter.h"
#include "sexp/TextWriter.h"
#include "utils/backwards_iterator.h"

#include <iostream>

// The following turn on logging sections, functions in the decompression
// algorithm.
#define LOG_SECTIONS 0
#define LOG_FUNCTIONS 0
// The following logs lookahead on each call to eval.
#define LOG_EVAL_LOOKAHEAD 0

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

static constexpr size_t DefaultStackSize = 256;

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
      Trace(ReadPos, WritePos, "InterpSexp"),
      FrameStack(Frame) {
  DefaultFormat = Symtab->create<Varuint64NoArgsNode>();
  CurSectionName.reserve(MaxExpectedSectionNameSize);
  FrameStack.reserve(DefaultStackSize);
  ParamStack.reserve(DefaultStackSize);
  ReturnStack.reserve(DefaultStackSize);
  EvalStack.reserve(DefaultStackSize);
}

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

const Node* Interpreter::getParam(const Node* P) {
  if (EvalStack.empty())
    fatal("Not inside a call frame, can't evaluate parameter accessor");
  assert(isa<ParamNode>(P));
  auto* Param = cast<ParamNode>(P);
  // define in terms of kid index in caller.
  IntType ParamIndex = Param->getValue() + 1;
  SymbolNode* DefiningSym = Param->getDefiningSymbol();
  for (const auto* Caller : backwards<ConstNodeVectorType>(EvalStack)) {
    assert(isa<EvalNode>(Caller));
    const EvalNode* Eval = cast<EvalNode>(Caller);
    if (DefiningSym != Eval->getCallName())
      continue;
    if (ParamIndex < IntType(Caller->getNumKids()))
      return Caller->getKid(ParamIndex);
  }
  fatal("Can't evaluate parameter reference");
  // Not reachable.
  return P;
}

IntType Interpreter::eval(const Node* Nd) {
  // TODO(kschimpf): Fix for ast streams.
  // TODO(kschimpf) Handle blocks.
  TRACE_METHOD("eval");
  TRACE_SEXP(nullptr, Nd);
#if LOG_EVAL_LOOKAHEAD
  TRACE_BLOCK({
    decode::ReadCursor Lookahead(ReadPos);
    fprintf(TRACE.indent(), "Lookahead:");
    for (int i = 0; i < 10; ++i) {
      if (!Lookahead.atByteEob())
        fprintf(TRACE.getFile(), " %x", Lookahead.readByte());
    }
    fprintf(TRACE.getFile(), " ");
    fprintf(ReadPos.describe(TRACE.getFile(), true), "\n");
  });
#endif
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
    case OpFile:
    case OpSection:
    case OpUndefine:
    case OpRename:
    case OpVersion:
    case OpUnknownSection:

      fprintf(stderr, "Evaluating not allowed: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      break;
    case OpParam:
      ReturnValue = eval(getParam(Nd));
      break;
    case OpDefine:
      ReturnValue = eval(Nd->getKid(2));
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
      TRACE_BLOCK({
        fprintf(TRACE.indent(), " Function %" PRIuMAX "\n",
                uintmax_t(LogBlockCount));
        if (LOG_NUMBERED_BLOCK && LogBlockCount == LOG_FUNCTION_NUMBER)
          TRACE.setTraceProgress(true);
      });
#endif
      decompressBlock(Nd->getKid(0));
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCKS
#if LOG_NUMBERED_BLOCK
      TRACE_BLOCK({
        if (LogBlockCount == LOG_FUNCTION_NUMBER)
          TRACE.setTraceProgress(0);
      });
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
    case OpEval: {
      auto* Sym = dyn_cast<SymbolNode>(Nd->getKid(0));
      assert(Sym);
      auto* Defn = dyn_cast<DefineNode>(Sym->getDefineDefinition());
      assert(Defn);
      auto* NumParams = dyn_cast<ParamNode>(Defn->getKid(1));
      assert(NumParams);
      int NumCallArgs = Nd->getNumKids() - 1;
      if (NumParams->getValue() != IntType(NumCallArgs)) {
        fprintf(Trace.getFile(), "Definition %s expects %" PRIuMAX
                                 "parameters, found: %" PRIuMAX "\n",
                Sym->getStringName().c_str(), uintmax_t(NumParams->getValue()),
                uintmax_t(NumCallArgs));
        fatal("Unable to evaluate call");
      }
      EvalStack.push_back(Nd);
      ReturnValue = eval(Defn);
      EvalStack.pop_back();
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
      while (!ReadPos.atReadBitEob()) {
        for (auto* Kid : *Nd)
          eval(Kid);
      }
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
  TRACE(IntType, "return value", ReturnValue);
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
  TRACE_METHOD("readOpcode");
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Illegal opcode selector: %s\n", getNodeTypeName(Type));
      fatal("Unable to read opcode");
      break;
    case OpOpcode: {
      const auto* Sel = cast<OpcodeNode>(Nd);
      const Node* SelectorNd = Sel->getKid(0);
      uint32_t SelectorSize = readOpcodeSelector(SelectorNd, LastReadValue);
      TRACE(uint32_t, "selector value", LastReadValue);
      if (NumOpcodes > 0) {
        TRACE(IntType, "prefix value", PrefixValue);
        TRACE(uint32_t, "selector bitsize", SelectorSize);
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
  TRACE(IntType, "return value", LastReadValue);
  return LastReadValue;
}

IntType Interpreter::read(const Node* Nd) {
  Call(InterpMethod::Read, Nd);
  runMethods();
  IntType Value = ReturnStack.back();
  ReturnStack.pop_back();
  return Value;
}

IntType Interpreter::write(IntType Value, const wasm::filt::Node* Nd) {
  Call(InterpMethod::Write, Nd);
  ParamStack.push_back(Value);
  runMethods();
  assert(Value == ReturnStack.back());
  ReturnStack.pop_back();
  return Value;
}

void Interpreter::runMethods() {
  TRACE_METHOD("runMethods");
#if 0
  CallFrame Frame;
#endif
  int count = 0;
  while (!FrameStack.empty()) {
    if (count++ >= 10)
      fatal("Too many iterations!");
    TRACE(int, "Method", int(Frame.Method));
    TRACE_SEXP("Code", Frame.Nd);
    switch (Frame.Method) {
      case InterpMethod::InterpMethod_NO_SUCH_METHOD:
        assert(false);
        fatal(
            "An unrecoverable error has occured in Interpreter::runMethods()");
        break;
      case InterpMethod::Eval:
        fatal("Eval/Read not yet implemented in runMethods");
        break;
      case InterpMethod::Read: {
        switch (NodeType Type = Frame.Nd->getType()) {
          default:
            fprintf(stderr, "Read not implemented: %s\n",
                    getNodeTypeName(Type));
            fatal("Read not implemented");
            pushReadReturnValue(0);
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const:
            pushReadReturnValue(dyn_cast<IntegerNode>(Frame.Nd)->getValue());
            break;
          case OpPeek: {
            // TODO(karlschimpf) Remove nested read.
            ReadCursor InitialPos(ReadPos);
            LastReadValue = read(Frame.Nd->getKid(0));
            ReadPos.swap(InitialPos);
            pushReadReturnValue(LastReadValue);
            break;
          }
          case OpLastRead:
            pushReadReturnValue(LastReadValue);
            break;
          case OpUint8NoArgs:
            pushReadReturnValue(Reader->readUint8(ReadPos));
            break;
          case OpUint8OneArg:
            pushReadReturnValue(Reader->readUint8Bits(
                ReadPos, cast<Uint8OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpUint32NoArgs:
            pushReadReturnValue(Reader->readUint32(ReadPos));
            break;
          case OpUint32OneArg:
            pushReadReturnValue(Reader->readUint32Bits(
                ReadPos, cast<Uint32OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpUint64NoArgs:
            pushReadReturnValue(Reader->readUint64(ReadPos));
            break;
          case OpUint64OneArg:
            pushReadReturnValue(Reader->readUint64Bits(
                ReadPos, cast<Uint64OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpVarint32NoArgs:
            pushReadReturnValue(Reader->readVarint32(ReadPos));
            break;
          case OpVarint32OneArg:
            pushReadReturnValue(Reader->readVarint32Bits(
                ReadPos, cast<Varint32OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpVarint64NoArgs:
            pushReadReturnValue(Reader->readVarint64(ReadPos));
            break;
          case OpVarint64OneArg:
            pushReadReturnValue(Reader->readVarint64Bits(
                ReadPos, cast<Varint64OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpVaruint32NoArgs:
            pushReadReturnValue(Reader->readVaruint32(ReadPos));
            break;
          case OpVaruint32OneArg:
            pushReadReturnValue(Reader->readVaruint32Bits(
                ReadPos, cast<Varuint32OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpVaruint64NoArgs:
            pushReadReturnValue(Reader->readVaruint64(ReadPos));
            break;
          case OpVaruint64OneArg:
            pushReadReturnValue(Reader->readVaruint64Bits(
                ReadPos, cast<Varuint64OneArgNode>(Frame.Nd)->getValue()));
            break;
          case OpVoid:
            pushReadReturnValue(0);
            break;
        }
        break;
      }
      case InterpMethod::Write: {
        IntType Value = ParamStack.back();
        TRACE(IntType, "Value", Value);
        const Node* Nd = Frame.Nd;
        switch (NodeType Type = Nd->getType()) {
          default:
            fprintf(stderr, "Write not implemented: %s\n",
                    getNodeTypeName(Type));
            fatal("Write not implemented");
            break;
          case OpParam:
            Call(InterpMethod::Write, getParam(Nd));
            break;
          case OpUint8NoArgs:
            Writer->writeUint8(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpUint8OneArg:
            Writer->writeUint8Bits(Value, WritePos,
                                   cast<Uint8OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpUint32NoArgs:
            Writer->writeUint32(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpUint32OneArg:
            Writer->writeUint32Bits(Value, WritePos,
                                    cast<Uint32OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpUint64NoArgs:
            Writer->writeUint64(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpUint64OneArg:
            Writer->writeUint64Bits(Value, WritePos,
                                    cast<Uint64OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpVarint32NoArgs:
            Writer->writeVarint32(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpVarint32OneArg:
            Writer->writeVarint32Bits(Value, WritePos,
                                      cast<Varint32OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpVarint64NoArgs:
            Writer->writeVarint64(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpVarint64OneArg:
            Writer->writeVarint64Bits(Value, WritePos,
                                      cast<Varint64OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpVaruint32NoArgs:
            Writer->writeVaruint32(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpVaruint32OneArg:
            Writer->writeVaruint32Bits(
                Value, WritePos, cast<Varuint32OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpVaruint64NoArgs:
            Writer->writeVaruint64(Value, WritePos);
            popArgAndReturnValue(Value);
            break;
          case OpVaruint64OneArg:
            Writer->writeVaruint64Bits(
                Value, WritePos, cast<Varuint64OneArgNode>(Nd)->getValue());
            popArgAndReturnValue(Value);
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const:
          case OpMap:
          case OpPeek:
          case OpVoid:
            popArgAndReturnValue(Value);
            break;
          case OpOpcode: {
            // TODO(karlschimpf): Remove calls to write().
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
        break;
      }
    }
  }
}

void Interpreter::decompress() {
  TRACE_METHOD("decompress");
  LastReadValue = 0;
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  TRACE(hex_uint32_t, "magic number", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to decompress, did not find WASM binary magic number");
  Writer->writeUint32(MagicNumber, WritePos);
  Version = Reader->readUint32(ReadPos);
  TRACE(hex_uint32_t, "version", Version);
  if (Version != WasmBinaryVersion)
    fatal("Unable to decompress, WASM version number not known");
  Writer->writeUint32(Version, WritePos);

  while (!ReadPos.atByteEob())
    decompressSection();
  WritePos.freezeEof();
}

void Interpreter::decompressBlock(const Node* Code) {
  TRACE_METHOD("decompressBlock");
  const uint32_t OldSize = Reader->readBlockSize(ReadPos);
  TRACE(uint32_t, "block size", OldSize);
  Reader->pushEobAddress(ReadPos, OldSize);
  WriteCursor BlockStart(WritePos);
  Writer->writeFixedBlockSize(WritePos, 0);
  size_t SizeAfterSizeWrite = Writer->getStreamAddress(WritePos);
  evalOrCopy(Code);
  const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
  TRACE(uint32_t, "OldSize", OldSize);
  TRACE(uint32_t, "NewSize", NewSize);
  if (!MinimizeBlockSize) {
    Writer->writeFixedBlockSize(BlockStart, NewSize);
  } else {
    TRACE_MESSAGE("Moving block...");
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
  // If not defined, must be at end of section, and hence byte aligned.
  while (!ReadPos.atByteEob())
    Writer->writeUint8(Reader->readUint8(ReadPos), WritePos);
}

void Interpreter::decompressSection() {
  // TODO(kschimpf) Handle 'filter' sections specially (i.e. install).  This
  // includes calling "clearCaches" on all filter s-expressions to remove an
  // (optimizing) caches installed.
  TRACE_METHOD("decompressSection");
  LastReadValue = 0;
  assert(isa<ByteReadStream>(Reader.get()));
#if LOG_SECTIONS
  size_t SectionAddress = ReadPos.getCurByteAddress();
#endif
  readSectionName();
#if LOG_SECTIONS
  TRACE_BLOCK({
    fprintf(TRACE.indent(), "@%" PRIxMAX " section '%s'\n",
            uintmax_t(SectionAddress), CurSectionName.c_str());
  });
#endif
  TRACE(string, "name", CurSectionName);
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
