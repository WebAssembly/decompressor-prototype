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

// Defines a pointer to a byte stream (for reading) that can written a bit at a
// time.

#ifndef DECOMPRESSOR_SRC_STREAM_BITWRITECURSOR_H
#define DECOMPRESSOR_SRC_STREAM_BITWRITECURSOR_H

#include "stream/WriteCursor.h"

namespace wasm {

namespace decode {

class BitWriteCursor : public WriteCursor {
 public:
  typedef uint32_t WordType;

  BitWriteCursor();

  BitWriteCursor(std::shared_ptr<Queue> Que);

  BitWriteCursor(StreamType Type, std::shared_ptr<Queue> Que);

  BitWriteCursor(const BitWriteCursor& C);

  BitWriteCursor(const BitWriteCursor& C, size_t StartAddress);

  ~BitWriteCursor() OVERRIDE;

  void assign(const BitWriteCursor& C);

  BitWriteCursor& operator=(const BitWriteCursor& C) {
    assign(C);
    return *this;
  }

  void swap(BitWriteCursor& C);

  void writeByte(uint8_t Byte);
  void writeBit(uint8_t Bit);
  void alignToByte();

  void describeDerivedExtensions(FILE* File) OVERRIDE;

 private:
  WordType CurWord;
  unsigned NumBits;

  void initFields() {
    CurWord = 0;
    NumBits = 0;
  }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_BITWRITECURSOR_H
