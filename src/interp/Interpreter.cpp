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
      Writer(Output),
      MinimizeBlockSize(false),
      Trace(ReadPos, Writer.Pos, "InterpSexp"),
      BlockStartStack(BlockStart) {
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
  Writer.Stream->writeUint8(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeUint32(uint32_t Value) {
  Writer.Stream->writeUint32(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeUint64(uint64_t Value) {
  Writer.Stream->writeUint64(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeVarint32(int32_t Value) {
  Writer.Stream->writeVarint32(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeVarint64(int64_t Value) {
  Writer.Stream->writeVarint64(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeVaruint32(uint32_t Value) {
  Writer.Stream->writeVaruint32(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeVaruint64(uint64_t Value) {
  Writer.Stream->writeVaruint64(Value, Writer.Pos);
  return true;
}

bool Interpreter::writeFreezeEof() {
  Writer.Pos.freezeEof();
  return true;
}

bool Interpreter::isWriteToByteStream() const {
  return isa<ByteWriteStream>(Writer.Stream.get());
}

bool Interpreter::writeValue(IntType Value, const Node* Format) {
  if (Writer.Stream->writeValue(LastReadValue, Writer.Pos, Format))
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
      BlockStartStack.push(Writer.Pos);
      Writer.Stream->writeFixedBlockSize(Writer.Pos, 0);
      BlockStartStack.push(Writer.Pos);
      return true;
    case PredefinedSymbol::BlockExit:
      if (MinimizeBlockSize) {
        // Mimimized block. Backpatch new size of block. If needed, move
        // block to fill gap between fixed and variable widths for block
        // size.
        WriteCursor WriteAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Writer.Stream->getBlockSize(BlockStart, Writer.Pos);
        TRACE(uint32_t, "New block size", NewSize);
        Writer.Stream->writeVarintBlockSize(BlockStart, NewSize);
        size_t SizeAfterBackPatch = Writer.Stream->getStreamAddress(BlockStart);
        size_t SizeAfterSizeWrite =
            Writer.Stream->getStreamAddress(WriteAfterSizeWrite);
        size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
        if (Diff) {
          size_t CurAddress = Writer.Stream->getStreamAddress(Writer.Pos);
          Writer.Stream->moveBlock(BlockStart, SizeAfterSizeWrite,
                                   (CurAddress - Diff) - SizeAfterBackPatch);
          Writer.Pos.swap(BlockStart);
        }
      } else {
        // Non-minimized block. Just backpatch in new size.
        WriteCursor WriteAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Writer.Stream->getBlockSize(BlockStart, Writer.Pos);
        TRACE(uint32_t, "New block size", NewSize);
        Writer.Stream->writeFixedBlockSize(BlockStart, NewSize);
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
