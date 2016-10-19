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

#include "interp/ByteReadStream.h"
#include "interp/ByteWriteStream.h"
#include "sexp/TextWriter.h"
#include "utils/backwards_iterator.h"

#include <iostream>

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

Interpreter::Interpreter(std::shared_ptr<Queue> Input,
                         std::shared_ptr<Queue> Output,
                         std::shared_ptr<SymbolTable> Symtab)
    : Reader(Input, Symtab, Trace),
      Symtab(Symtab),
      WritePos(StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      MinimizeBlockSize(false),
      Trace(ReadPos, WritePos, "InterpSexp"),
      BlockStartStack(BlockStart) {
  DefaultFormat = Symtab->getVaruint64Definition();
}

void Interpreter::describeBlockStartStack(FILE* File) {
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& Pos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void Interpreter::describeAllNonemptyStacks(FILE* File) {
  Reader::describeAllNonemptyStacks(File);
  if (!BlockStartStack.empty())
    describeBlockStartStack(File);
}

void Interpreter::clearStacksExceptFrame() {
  BlockStart = WriteCursor();
  BlockStartStack.clear();
}

bool Interpreter::writeUint8(uint8_t Value) {
  Writer->writeUint8(Value, WritePos);
  return true;
}

bool Interpreter::writeUint32(uint32_t Value) {
  Writer->writeUint32(Value, WritePos);
  return true;
}

bool Interpreter::writeUint64(uint64_t Value) {
  Writer->writeUint64(Value, WritePos);
  return true;
}

bool Interpreter::writeVarint32(int32_t Value) {
  Writer->writeVarint32(Value, WritePos);
  return true;
}

bool Interpreter::writeVarint64(int64_t Value) {
  Writer->writeVarint64(Value, WritePos);
  return true;
}

bool Interpreter::writeVaruint32(uint32_t Value) {
  Writer->writeVaruint32(Value, WritePos);
  return true;
}

bool Interpreter::writeVaruint64(uint64_t Value) {
  Writer->writeVaruint64(Value, WritePos);
  return true;
}

bool Interpreter::writeFreezeEof() {
  WritePos.freezeEof();
  return true;
}

bool Interpreter::isWriteToByteStream() const {
  return isa<ByteWriteStream>(Writer.get());
}

bool Interpreter::writeValue(IntType Value, const Node* Format) {
  if (Writer->writeValue(LastReadValue, WritePos, Format))
    return true;
  failCantWrite();
  return false;
}

bool Interpreter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr) {
    failNotImplemented();
    return false;
  }
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::BlockEnter:
      BlockStartStack.push(WritePos);
      Writer->writeFixedBlockSize(WritePos, 0);
      BlockStartStack.push(WritePos);
      return true;
    case PredefinedSymbol::BlockExit:
      if (MinimizeBlockSize) {
        // Mimimized block. Backpatch new size of block. If needed, move
        // block to fill gap between fixed and variable widths for block
        // size.
        WriteCursor WritePosAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
        TRACE(uint32_t, "New block size", NewSize);
        Writer->writeVarintBlockSize(BlockStart, NewSize);
        size_t SizeAfterBackPatch = Writer->getStreamAddress(BlockStart);
        size_t SizeAfterSizeWrite =
            Writer->getStreamAddress(WritePosAfterSizeWrite);
        size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
        if (Diff) {
          size_t CurAddress = Writer->getStreamAddress(WritePos);
          Writer->moveBlock(BlockStart, SizeAfterSizeWrite,
                            (CurAddress - Diff) - SizeAfterBackPatch);
          WritePos.swap(BlockStart);
        }
      } else {
        // Non-minimized block. Just backpatch in new size.
        WriteCursor WritePosAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
        TRACE(uint32_t, "New block size", NewSize);
        Writer->writeFixedBlockSize(BlockStart, NewSize);
      }
      BlockStartStack.pop();
      return true;
    default:
      break;
  }
  // Ignore other actions.
  return true;
}

void Interpreter::decompress() {
  start();
  readBackFilled();
}

}  // end of namespace interp.

}  // end of namespace wasm.
