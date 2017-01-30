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

#include "stream/BitWriteCursor.h"

namespace wasm {

namespace decode {

namespace {

constexpr BitWriteCursor::WordType BitsInByte =
    BitWriteCursor::WordType(sizeof(uint8_t) * CHAR_BIT);

}  // end of namespace

BitWriteCursor::BitWriteCursor() {
  initFields();
}

BitWriteCursor::BitWriteCursor(std::shared_ptr<Queue> Que) : WriteCursor(Que) {
  initFields();
}

BitWriteCursor::BitWriteCursor(StreamType Type, std::shared_ptr<Queue> Que)
    : WriteCursor(Type, Que) {
  initFields();
}

BitWriteCursor::BitWriteCursor(const BitWriteCursor& C)
    : WriteCursor(C), CurWord(C.CurWord), NumBits(C.NumBits) {
}

BitWriteCursor::BitWriteCursor(const BitWriteCursor& C, size_t StartAddress)
    : WriteCursor(C, StartAddress), CurWord(C.CurWord), NumBits(C.NumBits) {
}

BitWriteCursor::~BitWriteCursor() {
}

bool BitWriteCursor::atEof() const OVERRIDE {
  if (!WriteCursor::atEof())
    return false;
  return NumBits == 0;
}

void BitWriteCursor::assign(const BitWriteCursor& C) {
  WriteCursor::assign(C);
  CurWord = C.CurWord;
  NumBits = C.NumBits;
}

void BitWriteCursor::swap(BitWriteCursor& C) {
  WriteCursor::swap(C);
  std::swap(CurWord, C.CurWord);
  std::swap(NumBits, C.NumBits);
}

void BitWriteCursor::writeByte(uint8_t Byte) {
  if (NumBits == 0)
    return WriteCursor::writeByte(Byte);
  CurWord = (CurWord << BitsInByte) | Byte;
  WriteCursor::writeByte(CurWord >> NumBits);
  CurWord &= (1 << WordType(NumBits)) - 1;
}

void BitWriteCursor::writeBit(uint8_t Bit) {
  assert(Bit <= 1);
  CurWord = (CurWord << 1) | Bit;
  ++NumBits;
  if (NumBits >= BitsInByte) {
    NumBits -= BitsInByte;
    WriteCursor::writeByte(CurWord >> NumBits);
    CurWord &= (1 << WordType(NumBits)) - 1;
  }
}

void BitWriteCursor::alignToByte() {
  if (NumBits == 0)
    return;
  WriteCursor::writeByte(CurWord << (BitsInByte - NumBits));
  CurWord = 0;
  NumBits = 0;
}

void BitWriteCursor::describeDerivedExtensions(FILE* File) {
  if (NumBits > 0)
    fprintf(File, ":%u", NumBits);
}

}  // end of namespace decode

}  // end of namespace wasm
