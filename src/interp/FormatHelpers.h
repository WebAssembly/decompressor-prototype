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

// Defines helper methods for WASM formats.

#ifndef DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H
#define DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H

#include "utils/Defs.h"

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

template <class Type, class ReadCursor>
Type readLEB128(ReadCursor& Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  return readLEB128Loop<Type>(Pos, Shift, Chunk);
}

template <class Type, class ReadCursor>
Type readSignedLEB128(ReadCursor& Pos) {
  uint32_t Shift;
  uint8_t Chunk;
  Type Value = readLEB128Loop<Type>(Pos, Shift, Chunk);
  if ((Chunk & 0x40) && (Shift < sizeof(Type) * CHAR_BIT))
    Value |= ~Type(0) << Shift;
  return Value;
}

template <class ReadCursor>
uint8_t readUint8(ReadCursor& Pos) {
  return Pos.readByte();
}

template <class ReadCursor>
uint32_t readUint32(ReadCursor& Pos) {
  return fmt::readFixed<uint32_t>(Pos);
}

template <class ReadCursor>
uint64_t readUint64(ReadCursor& Pos) {
  return fmt::readFixed<uint64_t>(Pos);
}

template <class ReadCursor>
int32_t readVarint32(ReadCursor& Pos) {
  return fmt::readSignedLEB128<uint32_t>(Pos);
}

template <class ReadCursor>
int64_t readVarint64(ReadCursor& Pos) {
  return fmt::readSignedLEB128<uint64_t>(Pos);
}

template <class ReadCursor>
uint32_t readVaruint32(ReadCursor& Pos) {
  return fmt::readLEB128<uint32_t>(Pos);
}

template <class ReadCursor>
uint64_t readVaruint64(ReadCursor& Pos) {
  return fmt::readLEB128<uint64_t>(Pos);
}

// Define LEB128 writers.
#ifdef LEB128_LOOP_UNTIL
#error("LEB128_LOOP_UNTIL already defined!")
#else
#define LEB128_LOOP_UNTIL(EndCond) \
  do {                             \
    uint8_t Byte = Value & 0x7f;   \
    Value >>= 7;                   \
    if (EndCond) {                 \
      Pos.writeByte(Byte);         \
      break;                       \
    } else {                       \
      Pos.writeByte(Byte | 0x80);  \
    }                              \
  } while (true)

template <class Type, class WriteCursor>
void writeLEB128(Type Value, WriteCursor& Pos) {
  LEB128_LOOP_UNTIL(Value == 0);
}

template <class Type, class WriteCursor>
void writePositiveLEB128(Type Value, WriteCursor& Pos) {
  LEB128_LOOP_UNTIL(Value == 0 && !(Byte & 0x40));
}

template <class Type, class WriteCursor>
void writeNegativeLEB128(Type Value, WriteCursor& Pos) {
  LEB128_LOOP_UNTIL(Value == -1 && (Byte & 0x40));
}

template <class Type, class WriteCursor>
void writeFixedLEB128(Type Value, WriteCursor& Pos) {
  constexpr uint32_t BitsInWord = sizeof(Type) * CHAR_BIT;
  constexpr uint32_t ChunkSize = CHAR_BIT - 1;
  constexpr uint32_t ChunksInWord = (BitsInWord + ChunkSize - 1) / ChunkSize;
  uint32_t Count = 0;
  LEB128_LOOP_UNTIL(++Count == ChunksInWord);
}

#undef LEB128_LOOP_UNTIL

#endif

template <class Type, class WriteCursor>
void writeFixed(Type Value, WriteCursor& Pos) {
  constexpr uint32_t WordSize = sizeof(Type);
  constexpr Type Mask = (Type(1) << CHAR_BIT) - 1;
  for (uint32_t i = 0; i < WordSize; ++i) {
    Pos.writeByte(uint8_t(Value & Mask));
    Value >>= CHAR_BIT;
  }
}

template <class WriteCursor>
void writeUint8(uint8_t Value, WriteCursor& Pos) {
  Pos.writeByte(Value);
}

template <class WriteCursor>
void writeUint32(uint32_t Value, WriteCursor& Pos) {
  writeFixed(Value, Pos);
}

template <class WriteCursor>
void writeUint64(uint64_t Value, WriteCursor& Pos) {
  writeFixed(Value, Pos);
}

template <class WriteCursor>
void writeVarint32(int32_t Value, WriteCursor& Pos) {
  if (Value < 0)
    writeNegativeLEB128(Value, Pos);
  else
    writePositiveLEB128(Value, Pos);
}

template <class WriteCursor>
void writeVarint64(int64_t Value, WriteCursor& Pos) {
  if (Value < 0)
    writeNegativeLEB128(Value, Pos);
  else
    writePositiveLEB128(Value, Pos);
}

template <class WriteCursor>
void writeVaruint32(uint32_t Value, WriteCursor& Pos) {
  writeLEB128(Value, Pos);
}

template <class WriteCursor>
void writeVaruint64(uint64_t Value, WriteCursor& Pos) {
  writeLEB128(Value, Pos);
}

template <class WriteCursor>
void writeFixedVaruint32(uint32_t Value, WriteCursor& Pos) {
  writeFixedLEB128(Value, Pos);
}

}  // end of namespace fmt

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H
