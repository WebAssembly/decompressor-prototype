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

#include "sexp/Ast.h"
#include "stream/Cursor.h"

namespace wasm {

namespace interp {

class State;

class WriteStream {
  WriteStream(const WriteStream &) = delete;
  WriteStream &operator=(const WriteStream &) = delete;
public:
  void writeUint8(decode::WriteCursor &Pos, decode::IntType Value) {
    writeUint8(Pos, 8, Value);
  }
  virtual void writeUint8(decode::WriteCursor &Pos, decode::IntType NumBits,
                          decode::IntType Value) = 0;

  void writeUint32(decode::WriteCursor &Pos, decode::IntType Value) {
    writeUint32(Pos, 8, Value);
  }
  virtual void writeUint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                           decode::IntType Value) = 0;

  void writeUint64(decode::WriteCursor &Pos, decode::IntType Value) {
    writeUint64(Pos, 8, Value);
  }
  virtual void writeUint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                           decode::IntType Value) = 0;

  void writeVarint32(decode::WriteCursor &Pos, decode::IntType Value) {
    writeVarint32(Pos, 8, Value);
  }
  virtual void writeVarint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                             decode::IntType Value) = 0;

  void writeVarint64(decode::WriteCursor &Pos, decode::IntType Value) {
    return writeVarint64(Pos, 8, Value);
  }
  virtual void writeVarint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                             decode::IntType Value) = 0;

  void writeVaruint1(decode::WriteCursor &Pos, decode::IntType Value) {
    writeVaruint1(Pos, 8, Value);
  }
  virtual void writeVaruint1(decode::WriteCursor &Pos, decode::IntType NumBits,
                             decode::IntType Value) = 0;

  void writeVaruint7(decode::WriteCursor &Pos, decode::IntType Value) {
    writeVaruint7(Pos, 8, Value);
  }
  virtual void writeVaruint7(decode::WriteCursor &Pos, decode::IntType NumBits,
                             decode::IntType Value) = 0;

  void writeVaruint32(decode::WriteCursor &Pos, decode::IntType Value) {
    writeVaruint32(Pos, 8, Value);
  }
  virtual void writeVaruint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                              decode::IntType Value) = 0;

  void writeVaruint64(decode::WriteCursor &Pos, decode::IntType Value) {
    writeVaruint64(Pos, 8, Value);
  }
  virtual void writeVaruint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                              decode::IntType Value) = 0;

protected:
  WriteStream() {}
};

class ByteWriteStream final : public WriteStream {
  ByteWriteStream(const WriteStream &) = delete;
  ByteWriteStream &operator=(const ByteWriteStream &) = delete;
public:
  ByteWriteStream() {}
  void writeUint8(decode::WriteCursor &Pos, decode::IntType NumBits,
                  decode::IntType Value) override;
  void writeUint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                   decode::IntType Value) override;
  void writeUint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                   decode::IntType Value) override;
  void writeVarint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                     decode::IntType Value) override;
  void writeVarint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                     decode::IntType Value) override;
  void writeVaruint1(decode::WriteCursor &Pos, decode::IntType NumBits,
                     decode::IntType Value) override;
  void writeVaruint7(decode::WriteCursor &Pos, decode::IntType NumBits,
                        decode::IntType Value) override;
  void writeVaruint32(decode::WriteCursor &Pos, decode::IntType NumBits,
                      decode::IntType Value) override;
  void writeVaruint64(decode::WriteCursor &Pos, decode::IntType NumBits,
                      decode::IntType Value) override;
};


} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
