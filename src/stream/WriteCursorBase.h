// -*- C++ -*-
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

// Defines the base class of a pointer to a byte stream for writing.

#ifndef DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H
#define DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H

#include "stream/Cursor.h"

namespace wasm {

namespace decode {

class WriteCursorBase : public Cursor {
 public:
  // Note: The nullary write cursor should not be used until it has been
  // assigned a value.
  WriteCursorBase() : Cursor() {}

  WriteCursorBase(std::shared_ptr<Queue> Que) : Cursor(StreamType::Byte, Que) {}

  WriteCursorBase(StreamType Type, std::shared_ptr<Queue> Que)
      : Cursor(Type, Que) {}

  explicit WriteCursorBase(const WriteCursorBase& C) : Cursor(C) {}
  WriteCursorBase(const Cursor& C, size_t StartAddress)
      : Cursor(C, StartAddress, false) {}

  virtual ~WriteCursorBase();

  WriteCursorBase& operator=(const WriteCursorBase& C) {
    assign(C);
    return *this;
  }

  BitsInByteType getBitsWritten() const { return CurByte.getBitsWritten(); }
  BitAddress getCurWriteBitAddress() const {
    BitAddress Address(CurAddress, getBitsWritten());
    return Address;
  }

  // Writes next byte. Fails if at end of file. NOTE: Assumed byte aligned!
  void writeByte(uint8_t Byte) {
    assert(isByteAligned());
    if (CurAddress < GuaranteedBeforeEob)
      writeOneByte(Byte);
    else
      writeFillWriteByte(Byte);
  }

  // Writes up to 32 bits to the output.
  void writeBits(uint32_t Value, uint32_t NumBits);

 protected:
  void writeOneByte(uint8_t Byte);

  virtual void writeFillWriteByte(uint8_t Byte) = 0;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H
