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

bool ReadCursor::fillBuffer() {
  if (CurAddress >= EobAddress)
    return false;
  size_t BufferSize;
  uint8_t *NewBuffer = Queue->getReadLockedPointer(
      CurAddress, ByteQueue::PageSize, BufferSize);
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

void WriteCursor::fillBuffer() {
  if (CurAddress >= EobAddress)
    fatal("Write past Eob");
  size_t BufferSize;
  uint8_t *NewBuffer = Queue->getWriteLockedPointer(
      CurAddress, ByteQueue::PageSize, BufferSize);
  releaseLock();
  if (BufferSize == 0)
    fatal("Write failed!\n");
  Buffer = NewBuffer;
  BufferEnd = Buffer + BufferSize;
  LockedAddress = CurAddress;
}

void WriteCursor::writeCurPage(FILE *File) {
  Queue->writePageAt(File, CurAddress);
}

} // end of namespace decode

} // end of namespace wasm
