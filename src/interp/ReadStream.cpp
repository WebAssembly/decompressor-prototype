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

namespace wasm {

using namespace decode;

namespace interp {

uint8_t ByteReadStream::readUint8(decode::ReadCursor &Pos,
                                  decode::IntType /*NumBits*/) {
  return Pos.readByte();
}

uint32_t ByteReadStream::readUint32(decode::ReadCursor &Pos,
                                    decode::IntType /*NumBits*/) {
  uint32_t Value = 0;
  constexpr uint32_t WordSize = sizeof(uint32_t);
  for (uint32_t i = 0; i < WordSize; ++i)
    Value = (Value << CHAR_BIT) | uint32_t(Pos.readByte());
  return Value;
}

int32_t ByteReadStream::readVarint32(decode::ReadCursor &Pos,
                                     decode::IntType /*NumBits*/) {
  uint32_t Value = 0;
  uint32_t Shift = 0;
  while (true) {
    uint32_t Chunk = Pos.readByte();
    uint32_t Data = Chunk & ~(uint8_t(1) << 7);
    Value |= Data << Shift;
    Shift += 7;
    if ((Chunk >> 7) == 0) {
      if (Shift < 32 && ((Data >> 6)  == 1))
        Value |= ~(uint32_t(1) << Shift);
      return int32_t(Value);
    }
  }
}

int32_t ByteReadStream::readVarint64(decode::ReadCursor &Pos,
                                     decode::IntType /*NumBits*/) {
  uint64_t Value = 0;
  uint64_t Shift = 0;
  while (true) {
    uint32_t Chunk = Pos.readByte();
    uint32_t Data = Chunk & ~(uint32_t(1) << 7);
    Value |= uint64_t(Data) << Shift;
    Shift += 7;
    if ((Chunk >> 7) == 0) {
      if (Shift < 64 && ((Data >> 6)  == 1))
        Value |= ~(uint64_t(1) << Shift);
      return int64_t(Value);
    }
  }
}

uint64_t ByteReadStream::readUint64(decode::ReadCursor &Pos,
                                    decode::IntType /*NumBits*/) {
  uint64_t Value = 0;
  constexpr uint32_t WordSize = sizeof(uint64_t);
  for (uint32_t i = 0; i < WordSize; ++i)
    Value = (Value << CHAR_BIT) | uint64_t(Pos.readByte());
  return Value;
}

uint8_t ByteReadStream::readVaruint1(decode::ReadCursor &Pos,
                                     decode::IntType /*NumBits*/) {
  return Pos.readByte() & ~(uint32_t(1) << 7);
}

uint8_t ByteReadStream::readVaruint7(decode::ReadCursor &Pos,
                                     decode::IntType /*NumBits*/) {
  return Pos.readByte() & ~(uint32_t(1) << 7);
}

uint32_t ByteReadStream::readVaruint32(decode::ReadCursor &Pos,
                                       decode::IntType /*NumBits*/) {
  uint32_t Value = 0;
  uint32_t Shift = 0;
  while (true) {
    uint32_t Chunk = Pos.readByte();
    uint32_t Data = Chunk & ~(uint32_t(1) << 7);
    Value |= Data << Shift;
    if ((Chunk >> 7) == 0)
      return Value;
    Shift += 7;
  }
}

uint64_t ByteReadStream::readVaruint64(decode::ReadCursor &Pos,
                                       decode::IntType /*NumBits*/) {
  uint64_t Value = 0;
  uint64_t Shift = 0;
  while (true) {
    uint32_t Chunk = Pos.readByte();
    uint32_t Data = Chunk & ~(uint32_t(1) << 7);
    Value |= uint64_t(Data) << Shift;
    if ((Chunk >> 7) == 0)
      return Value;
    Shift += 7;
  }
}

} // end of namespace decode

} // end of namespace wasm
