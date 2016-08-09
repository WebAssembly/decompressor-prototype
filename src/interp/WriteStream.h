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

#ifndef DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
#define DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H

#include "sexp/Ast.h"
#include "stream/Cursor.h"
#include "utils/Casting.h"

#include <memory>

namespace wasm {

namespace interp {

class State;

class WriteStream : public std::enable_shared_from_this<WriteStream> {
  WriteStream(const WriteStream&) = delete;
  WriteStream& operator=(const WriteStream&) = delete;

 public:
  void writeUint8(uint8_t Value, decode::Cursor& Pos) {
    writeUint8Bits(Value, Pos, 8);
  }
  virtual void writeUint8Bits(uint8_t,
                              decode::Cursor& Pos,
                              uint32_t NumBits) = 0;

  void writeUint32(uint32_t Value, decode::Cursor& Pos) {
    writeUint32Bits(Value, Pos, 8);
  }
  virtual void writeUint32Bits(uint32_t Value,
                               decode::Cursor& Pos,
                               uint32_t NumBits) = 0;

  void writeUint64(uint64_t Value, decode::Cursor& Pos) {
    writeUint64Bits(Value, Pos, 8);
  }
  virtual void writeUint64Bits(uint64_t Value,
                               decode::Cursor& Pos,
                               uint32_t NumBits) = 0;

  void writeVarint32(int32_t Value, decode::Cursor& Pos) {
    writeVarint32Bits(Value, Pos, 8);
  }
  virtual void writeVarint32Bits(int32_t Value,
                                 decode::Cursor& Pos,
                                 uint32_t NumBits) = 0;

  void writeVarint64(int64_t Value, decode::Cursor& Pos) {
    return writeVarint64Bits(Value, Pos, 8);
  }
  virtual void writeVarint64Bits(int64_t Value,
                                 decode::Cursor& Pos,
                                 uint32_t NumBits) = 0;

  void writeVaruint32(uint32_t Value, decode::Cursor& Pos) {
    writeVaruint32Bits(Value, Pos, 8);
  }
  virtual void writeVaruint32Bits(uint32_t Value,
                                  decode::Cursor& Pos,
                                  uint32_t NumBits) = 0;

  void writeVaruint64(uint64_t Value, decode::Cursor& Pos) {
    writeVaruint64Bits(Value, Pos, 8);
  }

  virtual void writeVaruint64Bits(uint64_t Value,
                                  decode::Cursor& Pos,
                                  uint32_t NumBits) = 0;

  virtual void alignToByte(decode::Cursor& Pos) = 0;

  // The following virtuals are used to implement blocks.

  // Returns stream specific address (i.e. bit address for bit streams, byte
  // address for byte streams, and int address for int streams).
  virtual size_t getStreamAddress(decode::Cursor& Pos) = 0;

  // Saves the block size using a fixed format that is independent of
  // the block size.
  virtual void writeFixedBlockSize(decode::Cursor& Pos, size_t BlockSize) = 0;
  virtual void writeVarintBlockSize(decode::Cursor& Pos, size_t BlockSIze) = 0;

  // Returns the size of the block, defined by the range of the
  // passed positions (specific to the stream).
  virtual size_t getBlockSize(decode::Cursor& StartPos,
                              decode::Cursor& EndPos) = 0;

  // Moves Size elemens (stream specific) to StartAddress.
  virtual void moveBlock(decode::Cursor& Pos,
                         size_t StartAddress,
                         size_t Size) = 0;

  decode::StreamType getType() const { return Type; }

  decode::StreamType getRtClassId() const { return Type; }
  static bool implementsClass(decode::StreamType /*RtClassId*/) { return true; }

 protected:
  WriteStream(decode::StreamType Type) : Type(Type) {}
  decode::StreamType Type;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
