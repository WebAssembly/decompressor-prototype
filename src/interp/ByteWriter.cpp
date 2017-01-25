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

// implements a stream writer for wasm/casm files.

#include "interp/ByteWriter.h"

#include "interp/ByteWriteStream.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

ByteWriter::ByteWriter(std::shared_ptr<decode::Queue> Output)
    : Pos(StreamType::Byte, Output),
      Stream(std::make_shared<ByteWriteStream>()),
      BlockStartStack(BlockStart) {
}

ByteWriter::~ByteWriter() {
}

void ByteWriter::reset() {
  BlockStart = BitWriteCursor();
  BlockStartStack.clear();
}

BitWriteCursor& ByteWriter::getPos() {
  return Pos;
}

TraceClass::ContextPtr ByteWriter::getTraceContext() {
  return Pos.getTraceContext();
}

const char* ByteWriter::getDefaultTraceName() const {
  return "ByteWriter";
}

decode::StreamType ByteWriter::getStreamType() const {
  return Stream->getType();
}

bool ByteWriter::writeUint8(uint8_t Value) {
  Stream->writeUint8(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeUint32(uint32_t Value) {
  Stream->writeUint32(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeUint64(uint64_t Value) {
  Stream->writeUint64(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeVarint32(int32_t Value) {
  Stream->writeVarint32(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeVarint64(int64_t Value) {
  Stream->writeVarint64(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeVaruint32(uint32_t Value) {
  Stream->writeVaruint32(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeVaruint64(uint64_t Value) {
  Stream->writeVaruint64(Value, Pos);
  return Pos.isQueueGood();
}

bool ByteWriter::writeFreezeEof() {
  Pos.freezeEof();
  return Pos.isQueueGood();
}

bool ByteWriter::writeAction(const filt::SymbolNode* Action) {
  switch (Action->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_enter_writeonly:
      BlockStartStack.push(Pos);
      Stream->writeFixedBlockSize(Pos, 0);
      BlockStartStack.push(Pos);
      return true;
    case PredefinedSymbol::Block_exit:
    case PredefinedSymbol::Block_exit_writeonly:
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

void ByteWriter::describeBlockStartStack(FILE* File) {
  if (BlockStartStack.empty())
    return;
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& Pos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void ByteWriter::describeState(FILE* File) {
  describeBlockStartStack(File);
}

}  // end of namespace interp

}  // end of namespace wasm
