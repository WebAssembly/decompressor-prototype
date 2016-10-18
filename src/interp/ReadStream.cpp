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

// Implements defaults for stream readers.

#include "interp/ReadStream.h"

namespace wasm {

using namespace decode;
using namespace filt;


namespace interp {

namespace {

template <class Type>
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

template <class Type>
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

template <class Type>
Type readLEB128(ReadCursor& Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  return readLEB128Loop<Type>(Pos, Shift, Chunk);
}

template <class Type>
Type readSignedLEB128(ReadCursor& Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  Type Value = readLEB128Loop<Type>(Pos, Shift, Chunk);
  if ((Chunk & 0x40) && (Shift < sizeof(Type) * CHAR_BIT))
    Value |= ~Type(0) << Shift;
  return Value;
}

} // end of anonymous namespace

uint8_t ReadStream::readUint8(ReadCursor& Pos) {
  return Pos.readByte();
}

uint32_t ReadStream::readUint32(ReadCursor& Pos) {
  return readFixed<uint32_t>(Pos);
}

uint64_t ReadStream::readUint64(ReadCursor& Pos) {
  return readFixed<uint64_t>(Pos);
}

int32_t ReadStream::readVarint32(ReadCursor& Pos) {
  return readSignedLEB128<uint32_t>(Pos);
}

int64_t ReadStream::readVarint64(ReadCursor& Pos) {
  return readSignedLEB128<uint64_t>(Pos);
}

uint32_t ReadStream::readVaruint32(ReadCursor& Pos) {
  return readLEB128<uint32_t>(Pos);
}

uint64_t ReadStream::readVaruint64(ReadCursor& Pos) {
  return readLEB128<uint64_t>(Pos);
}

}  // end of namespace interp

}  // end of namespace wasm
