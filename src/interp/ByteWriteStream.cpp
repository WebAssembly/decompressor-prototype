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

// Implements a byte stream writer.

#include "interp/ByteWriteStream.h"
#include "stream/ReadCursor.h"

#include <iostream>

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

ByteWriteStream::~ByteWriteStream() {
}

bool ByteWriteStream::writeValue(IntType Value,
                                 WriteCursor& Pos,
                                 const Node* Format) {
  switch (Format->getType()) {
    case OpUint8:
      writeUint8(Value, Pos);
      return true;
    case OpUint32:
      writeUint32(Value, Pos);
      return true;
    case OpUint64:
      writeUint64(Value, Pos);
      return true;
    case OpVarint32:
      writeVarint32(Value, Pos);
      return true;
    case OpVarint64:
      writeVarint64(Value, Pos);
      return true;
    case OpVaruint32:
      writeVaruint32(Value, Pos);
      return true;
    case OpVaruint64:
      writeVaruint64(Value, Pos);
    default:
      return false;
  }
}

bool ByteWriteStream::writeAction(WriteCursor& Pos,
                                  const CallbackNode* Action) {
  return true;
}

size_t ByteWriteStream::getStreamAddress(WriteCursor& Pos) {
  return Pos.getCurByteAddress();
}

void ByteWriteStream::writeFixedBlockSize(WriteCursor& Pos, size_t BlockSize) {
  writeFixedVaruint32(BlockSize, Pos);
}

void ByteWriteStream::writeVarintBlockSize(decode::WriteCursor& Pos,
                                           size_t BlockSize) {
  writeVaruint32(BlockSize, Pos);
}

size_t ByteWriteStream::getBlockSize(decode::WriteCursor& StartPos,
                                     decode::WriteCursor& EndPos) {
  return EndPos.getCurByteAddress() -
         (StartPos.getCurByteAddress() + ChunksInWord);
}

void ByteWriteStream::moveBlock(decode::WriteCursor& Pos,
                                size_t StartAddress,
                                size_t Size) {
  ReadCursor CopyPos(Pos, StartAddress);
  for (size_t i = 0; i < Size; ++i)
    Pos.writeByte(CopyPos.readByte());
}

}  // end of namespace decode

}  // end of namespace wasm
