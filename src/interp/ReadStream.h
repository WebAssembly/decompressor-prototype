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

// Defines the API for all stream readers.

#ifndef DECOMPRESSOR_SRC_INTERP_READSTREAM_H
#define DECOMPRESSOR_SRC_INTERP_READSTREAM_H

#include "sexp/Ast.h"
#include "stream/Cursor.h"
#include "utils/Casting.h"

#include <memory>

namespace wasm {

namespace interp {

class State;

class ReadStream : public std::enable_shared_from_this<ReadStream> {
  ReadStream(const ReadStream&) = delete;
  ReadStream& operator=(const ReadStream&) = delete;

 public:
  uint8_t readUint8(decode::Cursor& Pos) { return readUint8Bits(Pos, 8); }
  virtual uint8_t readUint8Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  uint32_t readUint32(decode::Cursor& Pos) { return readUint32Bits(Pos, 8); }
  virtual uint32_t readUint32Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  uint64_t readUint64(decode::Cursor& Pos) { return readUint64Bits(Pos, 8); }
  virtual uint64_t readUint64Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  int32_t readVarint32(decode::Cursor& Pos) { return readVarint32Bits(Pos, 8); }
  virtual int32_t readVarint32Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  int64_t readVarint64(decode::Cursor& Pos) { return readVarint64Bits(Pos, 8); }
  virtual int64_t readVarint64Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  uint32_t readVaruint32(decode::Cursor& Pos) {
    return readVaruint32Bits(Pos, 8);
  }
  virtual uint32_t readVaruint32Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  uint64_t readVaruint64(decode::Cursor& Pos) {
    return readVaruint64Bits(Pos, 8);
  }
  virtual uint64_t readVaruint64Bits(decode::Cursor& Pos, uint32_t NumBits) = 0;

  // Align to nearest (next) byte boundary.
  virtual void alignToByte(decode::Cursor& Pos) = 0;

  // The following virtuals are used to implement blocks.

  // Reads in the stream specific block size.
  virtual size_t readBlockSize(decode::Cursor& Pos) = 0;

  // Sets Eob based on BlockSize (returned from readBlockSize), based
  // on stream specific block size.
  virtual void pushEobAddress(decode::Cursor& Pos, size_t BlockSize) = 0;

  decode::StreamType getType() const { return Type; }

  decode::StreamType getRtClassId() const { return Type; }

  static bool implementsClass(decode::StreamType /*RtClassID*/) {
    return false;
  }

 protected:
  ReadStream(decode::StreamType Type) : Type(Type) {}
  decode::StreamType Type;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_READSTREAM_H
