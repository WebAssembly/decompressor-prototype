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

#include "interp/ByteReadStream.h"

namespace {

using namespace wasm::decode;

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

}  // end of anonymous namespace

namespace wasm {

namespace interp {

uint8_t ByteReadStream::readUint8Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return Pos.readByte();
}

uint32_t ByteReadStream::readUint32Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readFixed<uint32_t>(Pos);
}

int32_t ByteReadStream::readVarint32Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readSignedLEB128<uint32_t>(Pos);
}

int64_t ByteReadStream::readVarint64Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readSignedLEB128<uint64_t>(Pos);
}

uint64_t ByteReadStream::readUint64Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readFixed<uint64_t>(Pos);
}

uint32_t ByteReadStream::readVaruint32Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readLEB128<uint32_t>(Pos);
}

uint64_t ByteReadStream::readVaruint64Bits(ReadCursor& Pos, uint32_t /*NumBits*/) {
  return readLEB128<uint64_t>(Pos);
}

void ByteReadStream::alignToByte(ReadCursor& /*Pos*/) {}

size_t ByteReadStream::readBlockSize(decode::ReadCursor& Pos) {
  return readVaruint32(Pos);
}

void ByteReadStream::pushEobAddress(decode::ReadCursor& Pos, size_t Address) {
  Pos.pushEobAddress(Pos.getCurByteAddress() + Address);
}

}  // end of namespace decode

}  // end of namespace wasm
