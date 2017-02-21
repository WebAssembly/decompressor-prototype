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
  BitWriteCursor(const BitWriteCursor& C, AddressType StartAddress);
  ~BitWriteCursor() OVERRIDE;
  bool atEof() const OVERRIDE;
  void assign(const BitWriteCursor& C);
  void swap(BitWriteCursor& C);
  void writeByte(ByteType Byte) OVERRIDE;
  void writeBit(ByteType Bit) OVERRIDE;
  void alignToByte();

  BitWriteCursor& operator=(const BitWriteCursor& C) {
    assign(C);
    return *this;
  }

  void describeDerivedExtensions(FILE* File, bool IncludeDetail) OVERRIDE;

 private:
  WordType CurWord;
  unsigned NumBits;
  void initFields();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_BITWRITECURSOR_H
