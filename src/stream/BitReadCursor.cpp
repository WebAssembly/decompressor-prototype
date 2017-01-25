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

// Implements a pionter to a byte stream (for reading) that can be read a bit at
// a time.

#include "stream/BitReadCursor.h"

namespace wasm {

namespace decode {

BitReadCursor::BitReadCursor() {
  initFields();
}

BitReadCursor::BitReadCursor(std::shared_ptr<Queue> Que)
    : ReadCursor(StreamType::Byte, Que) {
  initFields();
}

BitReadCursor::BitReadCursor(StreamType Type, std::shared_ptr<Queue> Que)
    : ReadCursor(Type, Que) {
  initFields();
}

BitReadCursor::BitReadCursor(const BitReadCursor& C)
    : ReadCursor(C), CurWord(C.CurWord), NumBits(C.NumBits) {
}

BitReadCursor::BitReadCursor(const BitReadCursor& C, size_t StartAddress)
    : ReadCursor(C, StartAddress), CurWord(C.CurWord), NumBits(C.NumBits) {
}

BitReadCursor::~BitReadCursor() {
}

void BitReadCursor::assign(const BitReadCursor& C) {
  ReadCursor::assign(C);
  CurWord = C.CurWord;
  NumBits = C.NumBits;
}

void BitReadCursor::swap(BitReadCursor& C) {
  ReadCursor::swap(C);
  CurWord = C.CurWord;
  NumBits = C.NumBits;
}

void BitReadCursor::alignToByte() {
  assert(NumBits < 8);
  NumBits = 0;
  CurWord = 0;
}

void BitReadCursor::describeDerivedExtensions(FILE* File) {
  if (NumBits > 0)
    fprintf(File, "+%u", NumBits);
}

#define BITREAD(Mask, MaskSize)                                      \
  do {                                                               \
    if (NumBits >= MaskSize) {                                       \
      NumBits -= MaskSize;                                           \
      uint8_t Value = uint8_t(CurWord >> NumBits);                   \
      CurWord &= ~Mask << NumBits;                                   \
      return Value;                                                  \
    }                                                                \
    if (atEob())                                                     \
      break;                                                         \
    /* Not enough bits left, read more in. */                        \
    CurWord = (CurWord << sizeof(uint8_t)) | ReadCursor::readByte(); \
    NumBits += sizeof(uint8_t);                                      \
  } while (1);                                                       \
  /* Leftover bits, fix (as best as possible) */                     \
  fail();                                                            \
  uint8_t Value = uint8_t(CurWord);                                  \
  CurWord = 0;                                                       \
  NumBits = 0;                                                       \
  return Value;

namespace {

static constexpr BitReadCursor::WordType ByteMask = (1 << sizeof(uint8_t)) - 1;

}  // end of anonymous namespace

uint8_t BitReadCursor::readByte() {
  if (NumBits == 0)
    return ReadCursor::readByte();
  BITREAD(ByteMask, sizeof(uint8_t));
}

uint8_t BitReadCursor::readBit() {
  BITREAD(1, 1);
}

}  // end of namespace decode

}  // end of namespace wasm
