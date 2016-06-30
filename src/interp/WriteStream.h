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

// Defines the API for all stream writers.

#ifndef DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
#define DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H

#include "interp/Streams.h"
#include "sexp/Ast.h"
#include "Casting.h"
#include "stream/Cursor.h"

namespace wasm {

namespace interp {

class State;

class WriteStream {
  WriteStream(const WriteStream &) = delete;
  WriteStream &operator=(const WriteStream &) = delete;
public:
  void writeUint8(uint8_t Value, decode::WriteCursor &Pos) {
    writeUint8(Value, Pos, 8);
  }
  virtual void writeUint8(uint8_t ,decode::WriteCursor &Pos,
                          uint32_t NumBits) = 0;

  void writeUint32(uint32_t Value, decode::WriteCursor &Pos) {
    writeUint32(Value, Pos, 8);
  }
  virtual void writeUint32(uint32_t Value, decode::WriteCursor &Pos,
                           uint32_t NumBits) = 0;

  void writeUint64(uint64_t Value, decode::WriteCursor &Pos) {
    writeUint64(Value, Pos, 8);
  }
  virtual void writeUint64(uint64_t Value, decode::WriteCursor &Pos,
                           uint32_t NumBits) = 0;

  void writeVarint32(int32_t Value, decode::WriteCursor &Pos) {
    writeVarint32(Value, Pos, 8);
  }
  virtual void writeVarint32(int32_t Value, decode::WriteCursor &Pos,
                             uint32_t NumBits) = 0;

  void writeVarint64(int64_t Value, decode::WriteCursor &Pos) {
    return writeVarint64(Value, Pos, 8);
  }
  virtual void writeVarint64(int64_t Value, decode::WriteCursor &Pos,
                             uint32_t NumBits) = 0;

  void writeVaruint32(uint32_t Value, decode::WriteCursor &Pos) {
    writeVaruint32(Value, Pos, 8);
  }
  virtual void writeVaruint32(uint32_t Value, decode::WriteCursor &Pos,
                              uint32_t NumBits) = 0;

  void writeVaruint64(uint64_t Value, decode::WriteCursor &Pos) {
    writeVaruint64(Value, Pos, 8);
  }
  virtual void writeVaruint64(uint64_t Value, decode::WriteCursor &Pos,
                              uint32_t NumBits) = 0;

  StreamType getType() const {
    return Type;
  }

  StreamType getRtClassId() const {
    return Type;
  }

  static bool implementsClass(StreamType /*RtClassId*/) {
    return true;
  }

protected:
  WriteStream(StreamType Type) : Type(Type) {}
  StreamType Type;
};

class ByteWriteStream final : public WriteStream {
  ByteWriteStream(const WriteStream &) = delete;
  ByteWriteStream &operator=(const ByteWriteStream &) = delete;
public:
  ByteWriteStream() : WriteStream(StreamType::Byte) {}
  void writeUint8(uint8_t Value, decode::WriteCursor &Pos,
                  uint32_t NumBits) override;
  void writeUint32(uint32_t Value, decode::WriteCursor &Pos,
                   uint32_t NumBits) override;
  void writeUint64(uint64_t Value, decode::WriteCursor &Pos,
                   uint32_t NumBits) override;
  void writeVarint32(int32_t Value, decode::WriteCursor &Pos,
                     uint32_t NumBits) override;
  void writeVarint64(int64_t Value, decode::WriteCursor &Pos,
                     uint32_t NumBits) override;
  void writeVaruint32(uint32_t Value, decode::WriteCursor &Pos,
                      uint32_t NumBits) override;
  void writeVaruint64(uint64_t Value, decode::WriteCursor &Pos,
                      uint32_t NumBits) override;

  void writeFixedVaruint32(uint32_t Value, decode::WriteCursor &Pos);

  static bool implementsClass(StreamType RtClassId) {
    return RtClassId == StreamType::Byte;
  }
};


} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
