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

// Implements template bodies.

#include "interp/FormatHelpers.h"

#include "stream/ReadCursor.h"

namespace wasm {

namespace interp {

namespace fmt {

template <class Type, class ReadCursor>
Type readFixed(ReadCursor& Pos) {
  Type Value = 0;
  constexpr uint32_t WordSize = sizeof(Type);
  uint32_t Shift = 0;
  for (uint32_t i = 0; i < WordSize; ++i) {
    Value |= Type(Pos.readByte()) << Shift;
    Shift += CHAR_BIT;
  }
  return Value;
}

template
uint32_t readFixed<uint32_t, decode::ReadCursor>(decode::ReadCursor&);

template
uint64_t readFixed<uint64_t, decode::ReadCursor>(decode::ReadCursor&);

template <class Type, class ReadCursor>
Type readLEB128Loop(ReadCursor& Pos, uint32_t& Shift, uint8_t& Chunk) {
  Type Value = 0;
  Shift = 0;
  while (true) {
    Chunk = Pos.readByte();
    Type Data = Chunk & ~(uint8_t(1) << 7);
    Value |= Data << Shift;
    Shift += 7;
    if ((Chunk >> 7) == 0)
      return Value;
  }
}

template
uint32_t readLEB128Loop<uint32_t, decode::ReadCursor>(
    decode::ReadCursor&, uint32_t&, uint8_t&);

template
uint64_t readLEB128Loop<uint64_t, decode::ReadCursor>(
    decode::ReadCursor&, uint32_t&, uint8_t&);

}  // end of namespace fmt

}  // end of namespace decode

}  // end of namespace wasm
