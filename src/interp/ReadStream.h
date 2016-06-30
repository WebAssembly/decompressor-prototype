/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Defines the API for all stream readers.

#ifndef DECOMPRESSOR_SRC_INTERP_READSTREAM_H
#define DECOMPRESSOR_SRC_INTERP_READSTREAM_H

#include "interp/Streams.h"
#include "sexp/Ast.h"
#include "Casting.h"
#include "stream/Cursor.h"

namespace wasm {

namespace interp {

class State;

class ReadStream {
  ReadStream(const ReadStream &) = delete;
  ReadStream &operator=(const ReadStream &) = delete;
public:
  uint8_t readUint8(decode::ReadCursor &Pos) {
    return readUint8(Pos, 8);
  }
  virtual uint8_t readUint8(decode::ReadCursor &Pos,
                            uint32_t NumBits) = 0;

  uint32_t readUint32(decode::ReadCursor &Pos) {
    return readUint32(Pos, 8);
  }
  virtual uint32_t readUint32(decode::ReadCursor &Pos,
                              uint32_t NumBits) = 0;

  uint64_t readUint64(decode::ReadCursor &Pos) {
    return readUint64(Pos, 8);
  }
  virtual uint64_t readUint64(decode::ReadCursor &Pos,
                              uint32_t NumBits) = 0;

  int32_t readVarint32(decode::ReadCursor &Pos) {
    return readVarint32(Pos, 8);
  }
  virtual int32_t readVarint32(decode::ReadCursor &Pos,
                               uint32_t NumBits) = 0;

  int64_t readVarint64(decode::ReadCursor &Pos) {
    return readVarint64(Pos, 8);
  }
  virtual int64_t readVarint64(decode::ReadCursor &Pos,
                               uint32_t NumBits) = 0;

  uint32_t readVaruint32(decode::ReadCursor &Pos) {
    return readVaruint32(Pos, 8);
  }
  virtual uint32_t readVaruint32(decode::ReadCursor &Pos,
                                 uint32_t NumBits) = 0;

  uint64_t readVaruint64(decode::ReadCursor &Pos) {
    return readVaruint64(Pos, 8);
  }
  virtual uint64_t readVaruint64(decode::ReadCursor &Pos,
                                 uint32_t NumBits) = 0;

  StreamType getType() const {
    return Type;
  }

  StreamType getRtClassId() const {
    return Type;
  }

  static bool implementsClass(StreamType /*RtClassID*/) {
    return true;
  }

protected:
  ReadStream(StreamType Type) : Type(Type) {}
  StreamType Type;
};

class ByteReadStream final : public ReadStream {
  ByteReadStream(const ReadStream &) = delete;
  ByteReadStream &operator=(const ByteReadStream &) = delete;
public:
  ByteReadStream() : ReadStream(StreamType::Byte) {}
  uint8_t readUint8(decode::ReadCursor &Pos,
                    uint32_t NumBits) override;
  uint32_t readUint32(decode::ReadCursor &Pos,
                      uint32_t NumBits) override;
  uint64_t readUint64(decode::ReadCursor &Pos,
                      uint32_t NumBits) override;
  int32_t readVarint32(decode::ReadCursor &Pos,
                       uint32_t NumBits) override;
  int64_t readVarint64(decode::ReadCursor &Pos,
                       uint32_t NumBits) override;
  uint32_t readVaruint32(decode::ReadCursor &Pos,
                         uint32_t NumBits) override;
  uint64_t readVaruint64(decode::ReadCursor &Pos,
                         uint32_t NumBits) override;
  static bool implementsClass(StreamType RtClassID) {
    return RtClassID == StreamType::Byte;
  }
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_INTERP_READSTREAM_H
