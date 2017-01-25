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

// Defines a pointer to a byte stream (for reading) that can read a bit
// at a time.

#ifndef DECOMPRESSOR_STC_STREAM_BITREADCURSOR_H
#define DECOMPRESSOR_STC_STREAM_BITREADCURSOR_H

#include "stream/ReadCursor.h"

namespace wasm {

namespace decode {

class BitReadCursor : public ReadCursor {
 public:
  typedef uint32_t WordType;
  BitReadCursor();
  BitReadCursor(std::shared_ptr<Queue> Que);
  BitReadCursor(StreamType Type, std::shared_ptr<Queue> Que);
  explicit BitReadCursor(const BitReadCursor& C);
  BitReadCursor(const BitReadCursor& C, size_t StartAddress);
  ~BitReadCursor() OVERRIDE;

  void assign(const BitReadCursor& C);

  BitReadCursor& operator=(const BitReadCursor& C) {
    assign(C);
    return *this;
  }

  void swap(BitReadCursor& C);

  uint8_t readByte();

  uint8_t readBit();

  void alignToByte();

  void describeDerivedExtensions(FILE* File) OVERRIDE;

 private:
  void initFields() {
    CurWord = 0;
    NumBits = 0;
  }
  WordType CurWord;
  unsigned NumBits;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_STC_STREAM_BITREADCURSOR_H
