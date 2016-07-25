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

void CursorImpl::jumpToByteAddress(size_t NewAddress) {
  if (PgCursor.isValidPageAddress(NewAddress)) {
    // We can reuse the same page, since NewAddress is within the page.
    PgCursor.setCurAddress(NewAddress);
    return;
  }
  // Move to the wanted page.
  Queue->Queue::read(NewAddress, 0, PgCursor);
}

bool CursorImpl::readFillBuffer() {
  size_t CurAddress = PgCursor.getCurAddress();
  if (CurAddress >= EobAddress)
    return false;
  size_t BufferSize = Queue->Queue::read(CurAddress, Page::Size, PgCursor);
  if (BufferSize == 0) {
    EobAddress = Queue->currentSize();
    return false;
  }
  return true;
}

void CursorImpl::writeFillBuffer() {
  size_t CurAddress = PgCursor.getCurAddress();
  if (CurAddress >= EobAddress)
    fatal("Write past Eob");
  size_t BufferSize = Queue->Queue::write(CurAddress, Page::Size, PgCursor);
  if (BufferSize == 0)
    fatal("Write failed!\n");
}

CursorImpl* CursorImpl::copy(StreamType WantedType) {
  assert(Type == WantedType);
  CursorImpl* Impl = new CursorImpl(WantedType, Queue, PgCursor);
  Impl->EobAddress = EobAddress;
  return Impl;
}

void WriteCursor::writeCurPage(FILE* File) {
  Impl->Queue->writePageAt(File, Impl->PgCursor.getCurAddress());
}

}  // end of namespace decode

}  // end of namespace wasm
