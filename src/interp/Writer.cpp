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

// implements a writer for wasm/casm files.

#include "interp/Writer.h"

#include "interp/ByteWriteStream.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

Writer::Writer(std::shared_ptr<decode::Queue> Output, TraceClassSexp& Trace)
    : Pos(StreamType::Byte, Output),
      Stream(std::make_shared<ByteWriteStream>()),
      Trace(Trace),
      MinimizeBlockSize(false),
      BlockStartStack(BlockStart) {
}

WriteCursor& Writer::getPos() {
  return Pos;
}

decode::StreamType Writer::getStreamType() const {
  return Stream->getType();
}

void Writer::reset() {
  BlockStart = WriteCursor();
  BlockStartStack.clear();
}

void Writer::describeBlockStartStack(FILE* File) {
  if (BlockStartStack.empty())
    return;
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& Pos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void Writer::describeState(FILE* File) {
  describeBlockStartStack(File);
}

bool Writer::writeUint8(uint8_t Value) {
  Stream->writeUint8(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeUint32(uint32_t Value) {
  Stream->writeUint32(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeUint64(uint64_t Value) {
  Stream->writeUint64(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeVarint32(int32_t Value) {
  Stream->writeVarint32(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeVarint64(int64_t Value) {
  Stream->writeVarint64(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeVaruint32(uint32_t Value) {
  Stream->writeVaruint32(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeVaruint64(uint64_t Value) {
  Stream->writeVaruint64(Value, Pos);
  return Pos.isQueueGood();
}

bool Writer::writeFreezeEof() {
  Pos.freezeEof();
  return Pos.isQueueGood();
}

bool Writer::writeValue(IntType Value, const Node* Format) {
  if (Stream->writeValue(Value, Pos, Format))
    return true;
  return false;
}

bool Writer::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::BlockEnter:
      BlockStartStack.push(Pos);
      Stream->writeFixedBlockSize(Pos, 0);
      BlockStartStack.push(Pos);
      return true;
    case PredefinedSymbol::BlockExit:
      if (MinimizeBlockSize) {
        // Mimimized block. Backpatch new size of block. If needed, move
        // block to fill gap between fixed and variable widths for block
        // size.
        WriteCursor WriteAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Stream->getBlockSize(BlockStart, Pos);
        TRACE(uint32_t, "New block size", NewSize);
        Stream->writeVarintBlockSize(BlockStart, NewSize);
        size_t SizeAfterBackPatch = Stream->getStreamAddress(BlockStart);
        size_t SizeAfterSizeWrite =
            Stream->getStreamAddress(WriteAfterSizeWrite);
        size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
        if (Diff) {
          size_t CurAddress = Stream->getStreamAddress(Pos);
          Stream->moveBlock(BlockStart, SizeAfterSizeWrite,
                            (CurAddress - Diff) - SizeAfterBackPatch);
          Pos.swap(BlockStart);
        }
      } else {
        // Non-minimized block. Just backpatch in new size.
        WriteCursor WriteAfterSizeWrite(BlockStart);
        BlockStartStack.pop();
        const size_t NewSize = Stream->getBlockSize(BlockStart, Pos);
        TRACE(uint32_t, "New block size", NewSize);
        Stream->writeFixedBlockSize(BlockStart, NewSize);
      }
      BlockStartStack.pop();
      return true;
    default:
      break;
  }
  // Ignore other actions.
  return true;
}

}  // end of namespace interp

}  // end of namespace wasm
