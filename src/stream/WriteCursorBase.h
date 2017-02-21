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

#ifndef DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H_
#define DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H_

#include "stream/Cursor.h"

namespace wasm {

namespace decode {

class WriteCursorBase : public Cursor {
 public:
  // Note: The nullary write cursor should not be used until it has been
  // assigned a value.
  WriteCursorBase();
  WriteCursorBase(std::shared_ptr<Queue> Que);
  WriteCursorBase(StreamType Type, std::shared_ptr<Queue> Que);
  explicit WriteCursorBase(const WriteCursorBase& C);
  WriteCursorBase(const Cursor& C, AddressType StartAddress);
  ~WriteCursorBase() OVERRIDE;
  // Writes next byte. Fails if at end of file.
  virtual void writeByte(ByteType Byte);
  virtual void writeBit(ByteType Bit);

  WriteCursorBase& operator=(const WriteCursorBase& C) {
    assign(C);
    return *this;
  }

 protected:
  void writeOneByte(ByteType Byte);
  virtual void writeFillWriteByte(ByteType Byte) = 0;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_WRITECURSORBASE_H_
