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

namespace wasm {

namespace decode {

bool CursorImpl::releaseLock() {
  if (Buffer == nullptr)
    return false;
  Queue->unlock(LockedAddress);
  Buffer = nullptr;
  BufferEnd = nullptr;
  return true;
}

bool CursorImpl::readFillBuffer() {
  if (CurAddress >= EobAddress)
    return false;
  size_t BufferSize;
  uint8_t* NewBuffer =
      Queue->getReadLockedPointer(CurAddress, Page::Size, BufferSize);
  releaseLock();
  if (BufferSize == 0) {
    EobAddress = Queue->currentSize();
    BufferEnd = Buffer = nullptr;
    return false;
  }
  Buffer = NewBuffer;
  BufferEnd = Buffer + BufferSize;
  LockedAddress = CurAddress;
  return true;
}

void CursorImpl::writeFillBuffer() {
  if (CurAddress >= EobAddress)
    fatal("Write past Eob");
  size_t BufferSize;
  uint8_t* NewBuffer =
      Queue->getWriteLockedPointer(CurAddress, Page::Size, BufferSize);
  releaseLock();
  if (BufferSize == 0)
    fatal("Write failed!\n");
  Buffer = NewBuffer;
  BufferEnd = Buffer + BufferSize;
  LockedAddress = CurAddress;
}

size_t CursorImpl::getCurBitAddress() const {
  return CurAddress * CHAR_BIT;
}

void CursorImpl::jumpToBitAddress(size_t NewAddress) {
  assert((NewAddress & (size_t(1) << CHAR_BIT)) == 0);
  jumpToByteAddress(NewAddress >> CHAR_BIT);
}

uint32_t CursorImpl::readBits(uint32_t NumBits) {
  fatal("CursorImpl::readBits not allowed for stream type!");
  return 0;
}

void CursorImpl::writeBits(uint8_t Value, uint32_t NumBits) {
  fatal("CursorImpl::writeBits not allowed for stream type!");
}

CursorImpl* CursorImpl::copy(StreamType WantedType) {
  assert(Type == WantedType);
  CursorImpl* Impl = new CursorImpl(WantedType, Queue);
  Impl->CurAddress = CurAddress;
  Impl->LockedAddress = LockedAddress;
  Queue->lock(LockedAddress);
  Impl->Buffer = Buffer;
  Impl->BufferEnd = BufferEnd;
  Impl->EobAddress = EobAddress;
  return Impl;
}

void WriteCursor::writeCurPage(FILE* File) {
  Impl->Queue->writePageAt(File, Impl->CurAddress);
}

}  // end of namespace decode

}  // end of namespace wasm
