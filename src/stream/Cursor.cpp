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

#include "stream/Cursor.h"

namespace {
#ifndef NDEBUG
static constexpr uint32_t MaxBitsAllowed = sizeof(uint32_t) * CHAR_BIT;
#endif
}

namespace wasm {

namespace decode {

void Cursor::jumpToByteAddress(size_t NewAddress) {
  if (isValidPageAddress(NewAddress)) {
    // We can reuse the same page, since NewAddress is within the page.
    setCurAddress(NewAddress);
    return;
  }
  // Move to the wanted page.
  Queue->readFromPage(NewAddress, 0, *this);
}

bool Cursor::readFillBuffer() {
  size_t CurAddress = getCurAddress();
  if (CurAddress >= getEobAddress())
    return false;
  size_t BufferSize = Queue->readFromPage(CurAddress, Page::Size, *this);
  if (BufferSize == 0) {
    setEobAddress(Queue->currentSize());
    return false;
  }
  return true;
}

void Cursor::writeFillBuffer() {
  size_t CurAddress = getCurAddress();
  if (CurAddress >= getEobAddress())
    fatal("Write past Eob");
  size_t BufferSize = Queue->writeToPage(CurAddress, Page::Size, *this);
  if (BufferSize == 0)
    fatal("Write failed!\n");
}

uint32_t Cursor::readBits(uint32_t NumBits) {
  assert(NumBits <= MaxBitsAllowed);
  uint32_t Value = 0;
  while (NumBits != 0) {
    if (NumBits <= CurByte.BitsInByteValue) {
      Value = (Value << NumBits) |
              (CurByte.ByteValue &
               (~uint32_t(0) >> (CurByte.BitsInByteValue - NumBits)));
      CurByte.ByteValue &= (uint32_t(1) << NumBits) - 1;
      CurByte.BitsInByteValue -= NumBits;
      return Value;
    }
    // Fill if empty.
    if (CurByte.BitsInByteValue == 0) {
      CurByte.setByte(readByte());
      continue;
    }
    // Use remaining byte value and continue.
    Value = (Value << CurByte.BitsInByteValue) | CurByte.ByteValue;
    NumBits -= CurByte.BitsInByteValue;
    CurByte.setByte(readByte());
  }
  return Value;
}

void Cursor::writeBits(uint32_t Value, uint32_t NumBits) {
  assert(NumBits <= MaxBitsAllowed);
  assert(Value == (~uint32_t(0) >> ((sizeof(uint32_t) * CHAR_BIT) - NumBits)));
  while (NumBits > 0) {
    const uint32_t Room = CHAR_BIT - CurByte.BitsInByteValue;
    if (Room >= NumBits) {
      CurByte.ByteValue = (CurByte.ByteValue << NumBits) | Value;
      CurByte.BitsInByteValue += NumBits;
      if (Room == NumBits) {
        writeByte(CurByte.ByteValue);
        CurByte.resetByte();
      }
      return;
    }
    CurByte.ByteValue = (CurByte.ByteValue << Room) | (Value >> Room);
    Value = Value | ((uint32_t(1) << Room) - 1);
    NumBits -= Room;
    writeByte(CurByte.ByteValue);
    CurByte.resetByte();
  }
}

}  // end of namespace decode

}  // end of namespace wasm
