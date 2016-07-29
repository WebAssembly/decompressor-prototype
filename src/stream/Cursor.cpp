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

void Cursor::jumpToByteAddress(size_t NewAddress) {
  if (PgCursor.isValidPageAddress(NewAddress)) {
    // We can reuse the same page, since NewAddress is within the page.
    PgCursor.setCurAddress(NewAddress);
    return;
  }
  // Move to the wanted page.
  Queue->readFromPage(NewAddress, 0, PgCursor);
}

bool Cursor::readFillBuffer() {
  size_t CurAddress = PgCursor.getCurAddress();
  if (CurAddress >= getEobAddress())
    return false;
  size_t BufferSize = Queue->readFromPage(CurAddress, Page::Size, PgCursor);
  if (BufferSize == 0) {
    setEobAddress(Queue->currentSize());
    return false;
  }
  return true;
}

void Cursor::writeFillBuffer() {
  size_t CurAddress = PgCursor.getCurAddress();
  if (CurAddress >= getEobAddress())
    fatal("Write past Eob");
  size_t BufferSize = Queue->writeToPage(CurAddress, Page::Size, PgCursor);
  if (BufferSize == 0)
    fatal("Write failed!\n");
}

void Cursor::writeCurPage(FILE* File) {
  Queue->writePageAt(File, PgCursor.getCurAddress());
}

}  // end of namespace decode

}  // end of namespace wasm
