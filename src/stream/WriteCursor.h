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

// Defines a pointer to a byte stream for writing.

#ifndef DECOMPRESSOR_SRC_STREAM_WRITECURSOR_H
#define DECOMPRESSOR_SRC_STREAM_WRITECURSOR_H

#include "stream/WriteCursorBase.h"

namespace wasm {

namespace decode {

class WriteCursor : public WriteCursorBase {
 public:
  // Note: The nullary write cursor should not be used until it has been
  // assigned a value.
  WriteCursor();
  WriteCursor(std::shared_ptr<Queue> Que);
  WriteCursor(StreamType Type, std::shared_ptr<Queue> Que);
  explicit WriteCursor(const WriteCursor& C);
  WriteCursor(const Cursor& C, size_t StartAddress);
  WriteCursor(const WriteCursorBase& C);
  ~WriteCursor() OVERRIDE;

  WriteCursor& operator=(const WriteCursor& C) {
    assign(C);
    return *this;
  }

 protected:
  void writeFillWriteByte(uint8_t Byte) OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_WRITECURSOR_H
