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

// Defines a byte stream reader.

#ifndef DECOMPRESSOR_SRC_INTERP_BYTEREADSTREAM_H
#define DECOMPRESSOR_SRC_INTERP_BYTEREADSTREAM_H

#include "interp/ReadStream.h"

namespace wasm {

namespace interp {

class ByteReadStream FINAL : public ReadStream {
  ByteReadStream(const ReadStream&) = delete;
  ByteReadStream& operator=(const ByteReadStream&) = delete;

 public:
  ByteReadStream() : ReadStream(decode::StreamType::Byte) {}
  uint8_t readUint8Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  uint32_t readUint32Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  uint64_t readUint64Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  int32_t readVarint32Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  int64_t readVarint64Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  uint32_t readVaruint32Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  uint64_t readVaruint64Bits(decode::ReadCursor& Pos, uint32_t NumBits) OVERRIDE;
  void alignToByte(decode::ReadCursor& Pos) OVERRIDE;
  size_t readBlockSize(decode::ReadCursor& Pos) OVERRIDE;
  void pushEobAddress(decode::ReadCursor& Pos, size_t BlockSize) OVERRIDE;
  static bool implementsClass(decode::StreamType RtClassID) {
    return RtClassID == decode::StreamType::Byte;
  }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif // DECOMPRESSOR_SRC_INTERP_BYTEREADSTREAM_H
