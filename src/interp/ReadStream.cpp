/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Implements defaults for stream readers.

#include "interp/ReadStream.h"
#include "interp/State.h"

namespace {

using namespace wasm::decode;

template<class Type>
Type readLEB128Loop(ReadCursor &Pos, uint32_t &Shift, uint8_t &Chunk) {
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

template<class Type>
Type readLEB128(ReadCursor &Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  return readLEB128Loop<Type>(Pos, Shift, Chunk);
}

template<class Type>
Type readSignedLEB128(ReadCursor &Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  Type Value = readLEB128Loop<Type>(Pos, Shift, Chunk);
  if (Chunk & 0x40)
    Value |= ~Type(0) << Shift;
  return Value;
}

} // end of anonymous namespace

namespace wasm {

using namespace decode;

namespace interp {

uint8_t ByteReadStream::readUint8(ReadCursor &Pos, uint32_t /*NumBits*/) {
  return Pos.readByte();
}

uint32_t ByteReadStream::readUint32(ReadCursor &Pos, uint32_t /*NumBits*/) {
  uint32_t Value = 0;
  constexpr uint32_t WordSize = sizeof(uint32_t);
  for (uint32_t i = 0; i < WordSize; ++i)
    Value = (Value << CHAR_BIT) | uint32_t(Pos.readByte());
  return Value;
}

int32_t ByteReadStream::readVarint32(ReadCursor &Pos, uint32_t /*NumBits*/) {
  return readSignedLEB128<uint32_t>(Pos);
}

int64_t ByteReadStream::readVarint64(ReadCursor &Pos, uint32_t /*NumBits*/) {
  return readSignedLEB128<uint64_t>(Pos);
}

uint64_t ByteReadStream::readUint64(ReadCursor &Pos, uint32_t /*NumBits*/) {
  uint64_t Value = 0;
  constexpr uint32_t WordSize = sizeof(uint64_t);
  for (uint32_t i = 0; i < WordSize; ++i)
    Value = (Value << CHAR_BIT) | uint64_t(Pos.readByte());
  return Value;
}

uint32_t ByteReadStream::readVaruint32(ReadCursor &Pos, uint32_t /*NumBits*/) {
  return readLEB128<uint32_t>(Pos);
}

uint64_t ByteReadStream::readVaruint64(ReadCursor &Pos, uint32_t /*NumBits*/) {
  return readLEB128<uint64_t>(Pos);
}

} // end of namespace decode

} // end of namespace wasm
