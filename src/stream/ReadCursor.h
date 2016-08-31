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

// Defines a pointer to a byte stream for reading.

#ifndef DECOMPRESSOR_SRC_STREAM_READCURSOR_H
#define DECOMPRESSOR_SRC_STREAM_READCURSOR_H

#include "stream/Cursor.h"

namespace wasm {

namespace decode {

class ReadCursor FINAL : public Cursor {
 public:
  // Note: The nullary read cursor should not be used until it has been assigned
  // a valid value.
  ReadCursor() : Cursor() {}

  ReadCursor(std::shared_ptr<Queue> Que) : Cursor(StreamType::Byte, Que) {}

  ReadCursor(StreamType Type, std::shared_ptr<Queue> Que) : Cursor(Type, Que) {}

  explicit ReadCursor(const Cursor& C) : Cursor(C) {}

  ReadCursor(const Cursor& C, size_t StartAddress)
      : Cursor(C, StartAddress, true) {}

  ~ReadCursor() {}

  ReadCursor& operator=(const ReadCursor& C) {
    assign(C);
    return *this;
  }

  bool atByteEob() {
    if (CurAddress < GuaranteedBeforeEob)
      return false;
    bool Result =
        CurAddress >= getEobAddress().getByteAddress() || !readFillBuffer();
    updateGuaranteedBeforeEob();
    return Result;
  }

  bool atReadBitEob() {
    // TODO(karlschimpf): Fix this to check at byte level!
    return atByteEob();
  }

  BitsInByteType getBitsRead() const { return CurByte.getBitsRead(); }

  BitAddress getCurReadBitAddress() const {
    BitAddress Address(CurAddress, getBitsRead());
    return Address;
  }

  void pushEobAddress(const BitAddress& NewValue) {
    EobPtr = std::make_shared<BlockEob>(NewValue, EobPtr);
    updateGuaranteedBeforeEob();
  }

  void popEobAddress() {
    EobPtr = EobPtr->getEnclosingEobPtr();
    assert(EobPtr);
    updateGuaranteedBeforeEob();
  }

  // Reads next byte. Returns zero if at end of file. NOTE: Assumes byte
  // aligned!
  uint8_t readByte() {
    assert(isByteAligned());
    if (CurAddress < GuaranteedBeforeEob)
      return readOneByte();
    return readByteAfterReadFill();
  }

  // Reads up to 32 bits from the input.
  uint32_t readBits(uint32_t NumBits);

  // Try to advance Distance bytes. Returns actual number of bytes advanced.  If
  // zero is returned (and Distance > 0), no more bytes are available to advance
  // on.
  size_t advance(size_t Distance);

 protected:
  uint8_t readOneByte() {
    uint8_t Byte = *getBufferPtr();
    ++CurAddress;
#if 0
    CurPage = Que->getReadPage(CurAddress);
    assert(CurPage);
#endif
    return Byte;
  }

  uint8_t readByteAfterReadFill();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_READCURSOR_H
