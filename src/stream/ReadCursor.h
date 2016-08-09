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
  ReadCursor() = delete;
  ReadCursor& operator=(const ReadCursor&) = delete;
 public:
  ReadCursor(StreamType Type, std::shared_ptr<Queue> Que)
      : Cursor(Type, Que) {}
  explicit ReadCursor(const ReadCursor& C) : Cursor(C) {}
  ~ReadCursor() {}

  bool atReadBitEob() { return getCurReadBitAddress() == getEobAddress(); }

  BitsInByteType getBitsRead() const { return CurByte.getBitsRead(); }

  BitAddress getCurReadBitAddress() const {
    BitAddress Address(getCurAddress(), getBitsRead());
    return Address;
  }

  void pushEobAddress(const BitAddress& NewValue) {
    EobPtr = std::make_shared<BlockEob>(NewValue, EobPtr);
  }

  void popEobAddress() {
    EobPtr = EobPtr->getEnclosingEobPtr();
    assert(EobPtr);
  }

  // Reads next byte. Returns zero if at end of file. NOTE: Assumes byte
  // aligned!
  uint8_t readByte() {
    assert(isByteAligned());
    if (isIndexAtEndOfPage() && !readFillBuffer())
      return 0;
    return PageCursor::readByte();
  }

  // Reads up to 32 bits from the input.
  uint32_t readBits(uint32_t NumBits);
};

}  // end of namespace decode

}  // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_READCURSOR_H
