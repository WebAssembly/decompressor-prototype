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

// Defines the API for all stream writers.

#ifndef DECOMPRESSOR_SRC_INTERP_BYTEWRITESTREAM_H
#define DECOMPRESSOR_SRC_INTERP_BYTEWRITESTREAM_H

#include "interp/WriteStream.h"

#include <memory>

namespace wasm {

namespace interp {

class ByteWriteStream FINAL : public WriteStream {
  ByteWriteStream(const WriteStream&) = delete;
  ByteWriteStream& operator=(const ByteWriteStream&) = delete;

 public:
  static constexpr uint32_t BitsInWord = sizeof(uint32_t) * CHAR_BIT;
  static constexpr uint32_t ChunkSize = CHAR_BIT - 1;
  static constexpr uint32_t ChunksInWord =
      (BitsInWord + ChunkSize - 1) / ChunkSize;
  ByteWriteStream() : WriteStream(decode::StreamType::Byte) {}

  bool writeValue(decode::IntType Value,
                  decode::WriteCursor& Pos,
                  const filt::Node* Format) OVERRIDE;
  bool writeAction(decode::WriteCursor& Pos,
                   const filt::CallbackNode* Action) OVERRIDE;
  size_t getStreamAddress(decode::WriteCursor& Pos) OVERRIDE;
  void writeFixedBlockSize(decode::WriteCursor& Pos, size_t BlockSize) OVERRIDE;
  void writeVarintBlockSize(decode::WriteCursor& Pos,
                            size_t BlockSIze) OVERRIDE;
  size_t getBlockSize(decode::WriteCursor& StartPos,
                      decode::WriteCursor& EndPos) OVERRIDE;
  void moveBlock(decode::WriteCursor& Pos,
                 size_t StartAddress,
                 size_t Size) OVERRIDE;

  static bool implementsClass(decode::StreamType RtClassId) {
    return RtClassId == decode::StreamType::Byte;
  }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_BYTEWRITESTREAM_H
