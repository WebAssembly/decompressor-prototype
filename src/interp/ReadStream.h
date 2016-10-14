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
#include "stream/ReadCursor.h"
#include "utils/Casting.h"

#include <memory>

namespace wasm {

namespace interp {

class State;

class ReadStream : public std::enable_shared_from_this<ReadStream> {
  ReadStream(const ReadStream&) = delete;
  ReadStream& operator=(const ReadStream&) = delete;

 public:

  virtual uint8_t readUint8(decode::ReadCursor& Pos) = 0;
  virtual uint32_t readUint32(decode::ReadCursor& Pos) = 0;
  virtual uint64_t readUint64(decode::ReadCursor& Pos) = 0;
  virtual int32_t readVarint32(decode::ReadCursor& Pos) = 0;
  virtual int64_t readVarint64(decode::ReadCursor& Pos) = 0;
  virtual uint32_t readVaruint32(decode::ReadCursor& Pos) = 0;

#if 0
  uint64_t readVaruint64(decode::ReadCursor& Pos) {
    return readVaruint64Bits(Pos, 8);
  }
  virtual uint64_t readVaruint64Bits(decode::ReadCursor& Pos,
                                     uint32_t NumBits) = 0;
#else
  virtual uint64_t readVaruint64(decode::ReadCursor& Pos) = 0;
#endif

  virtual decode::IntType readValue(decode::ReadCursor& Pos,
                                    const filt::Node* Format) = 0;

  // Align to nearest (next) byte boundary.
  virtual void alignToByte(decode::ReadCursor& Pos) = 0;

  // The following virtuals are used to implement blocks.

  // Reads in the stream specific block size.
  virtual size_t readBlockSize(decode::ReadCursor& Pos) = 0;

  // Sets Eob based on BlockSize (returned from readBlockSize), based
  // on stream specific block size.
  virtual void pushEobAddress(decode::ReadCursor& Pos, size_t BlockSize) = 0;

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
