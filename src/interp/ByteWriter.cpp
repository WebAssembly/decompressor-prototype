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
#include "interp/WriteStream.h"
#include "sexp/Ast.h"
#include "stream/WriteCursor.h"
#include "utils/Casting.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

ByteWriter::ByteWriter(std::shared_ptr<decode::Queue> Output)
    : Writer(true),
      WritePos(StreamType::Byte, Output),
      Stream(std::make_shared<ByteWriteStream>()),
      BlockStartStack(BlockStart) {
}

ByteWriter::~ByteWriter() {
}

void ByteWriter::reset() {
  BlockStart = BitWriteCursor();
  BlockStartStack.clear();
}

void ByteWriter::setPos(const decode::BitWriteCursor& NewPos) {
  WritePos = NewPos;
}

BitWriteCursor& ByteWriter::getPos() {
  return WritePos;
}

TraceContextPtr ByteWriter::getTraceContext() {
  return WritePos.getTraceContext();
}

const char* ByteWriter::getDefaultTraceName() const {
  return "ByteWriter";
}

decode::StreamType ByteWriter::getStreamType() const {
  return Stream->getType();
}

bool ByteWriter::writeBit(uint8_t Value) {
  Stream->writeBit(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeUint8(uint8_t Value) {
  Stream->writeUint8(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeUint32(uint32_t Value) {
  Stream->writeUint32(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeUint64(uint64_t Value) {
  Stream->writeUint64(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeVarint32(int32_t Value) {
  Stream->writeVarint32(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeVarint64(int64_t Value) {
  Stream->writeVarint64(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeVaruint32(uint32_t Value) {
  Stream->writeVaruint32(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeVaruint64(uint64_t Value) {
  Stream->writeVaruint64(Value, WritePos);
  return WritePos.isQueueGood();
}

bool ByteWriter::writeFreezeEof() {
  WritePos.freezeEof();
  return WritePos.isQueueGood();
}

bool ByteWriter::writeBinary(IntType Value, const Node* Encoding) {
  if (!isa<BinaryEvalNode>(Encoding))
    return false;
  const auto* Eval = cast<BinaryEvalNode>(Encoding);
  const Node* Enc = Eval->getEncoding(Value);
  if (!isa<BinaryAcceptNode>(Enc))
    return false;
  const auto* Accept = cast<BinaryAcceptNode>(Enc);
  unsigned NumBits = Accept->getNumBits();
  IntType Bits = Accept->getValue();
  while (NumBits) {
    --NumBits;
    WritePos.writeBit(uint8_t(Bits & 0x1));
    Bits >>= 1;
  }
  return true;
}

bool ByteWriter::alignToByte() {
  WritePos.alignToByte();
  return true;
}

bool ByteWriter::writeBlockEnter() {
  // Force alignment before processing, in case non-byte encodings
  // are used.
  alignToByte();
  BlockStartStack.push(WritePos);
  Stream->writeFixedBlockSize(WritePos, 0);
  BlockStartStack.push(WritePos);
  return true;
}

bool ByteWriter::writeBlockExit() {
  // Force alignment before processing, in case non-byte encodings
  // are used.
  WritePos.alignToByte();
  if (MinimizeBlockSize) {
    // Mimimized block. Backpatch new size of block. If needed, move
    // block to fill gap between fixed and variable widths for block
    // size.
    WriteCursor WriteAfterSizeWrite(BlockStart);
    BlockStartStack.pop();
    const size_t NewSize = Stream->getBlockSize(BlockStart, WritePos);
    TRACE(uint32_t, "New block size", NewSize);
    Stream->writeVarintBlockSize(BlockStart, NewSize);
    size_t SizeAfterBackPatch = Stream->getStreamAddress(BlockStart);
    size_t SizeAfterSizeWrite = Stream->getStreamAddress(WriteAfterSizeWrite);
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t CurAddress = Stream->getStreamAddress(WritePos);
      Stream->moveBlock(BlockStart, SizeAfterSizeWrite,
                        (CurAddress - Diff) - SizeAfterBackPatch);
      WritePos.swap(BlockStart);
    }
  } else {
    // Non-minimized block. Just backpatch in new size.
    WriteCursor WriteAfterSizeWrite(BlockStart);
    BlockStartStack.pop();
    const size_t NewSize = Stream->getBlockSize(BlockStart, WritePos);
    TRACE(uint32_t, "New block size", NewSize);
    Stream->writeFixedBlockSize(BlockStart, NewSize);
  }
  BlockStartStack.pop();
  return true;
}

void ByteWriter::describeBlockStartStack(FILE* File) {
  if (BlockStartStack.empty())
    return;
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& WritePos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(WritePos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void ByteWriter::describeState(FILE* File) {
  describeBlockStartStack(File);
}

}  // end of namespace interp

}  // end of namespace wasm
