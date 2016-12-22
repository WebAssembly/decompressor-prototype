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

// Reads text from an array.

#ifndef DECOMPRESSOR_SRC_STREAM_ARRAYREADER_H
#define DECOMPRESSOR_SRC_STREAM_ARRAYREADER_H

#include "stream/RawStream.h"

namespace wasm {

namespace decode {

class ArrayReader : public RawStream {
  ArrayReader() = delete;
  ArrayReader(const ArrayReader&) = delete;
  ArrayReader& operator=(const ArrayReader&) = delete;

 public:
  ArrayReader(const uint8_t* Buffer, const size_t BufferSize)
      : Buffer(Buffer), BufferSize(BufferSize), CurPosition(0) {}
  ~ArrayReader() OVERRIDE;

  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;
  bool hasErrors() OVERRIDE;

 protected:
  const uint8_t* Buffer;
  const size_t BufferSize;
  size_t CurPosition;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_ARRAYREADER_H
