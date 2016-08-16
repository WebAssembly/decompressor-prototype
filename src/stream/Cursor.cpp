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

#include "stream/Cursor.h"

namespace wasm {

namespace decode {

FILE* WorkingByte::describe(FILE* File) {
  fprintf(File, "[%u:%u] ", Value, BitsInValue);
  return File;
}

size_t Cursor::advance(size_t Distance) {
  size_t WantedAddress = CurAddress + Distance;
  size_t DistanceMoved = 0;
  while (CurAddress < WantedAddress && CurAddress < Que->getEofAddress()) {
    size_t Size = Que->readFromPage(CurAddress, Page::Size, *this);
    if (Size == 0)
      break;
    CurAddress += Size;
    DistanceMoved += Size;
    CurPage = Que->getPage(CurAddress);
  }
  return DistanceMoved;
}

bool Cursor::readFillBuffer() {
  size_t CurAddress = getCurAddress();
  if (CurAddress >= Que->getEofAddress())
    return false;
  size_t BufferSize = Que->readFromPage(CurAddress, Page::Size, *this);
  return BufferSize > 0;
}

void Cursor::writeFillBuffer() {
  size_t CurAddress = getCurAddress();
  if (CurAddress >= Que->getEofAddress())
    fatal("Write past Eof");
  size_t BufferSize = Que->writeToPage(CurAddress, Page::Size, *this);
  if (BufferSize == 0)
    fatal("Write failed!\n");
}

FILE* Cursor::describe(FILE* File, bool IncludeDetail) {
  if (IncludeDetail)
    fprintf(File, "Cursor<");
  PageCursor::describe(File, IncludeDetail);
  if (!CurByte.isEmpty())
    CurByte.describe(File);
  if (!IncludeDetail)
    return File;
  if (EobPtr->isDefined()) {
    fprintf(File, ", eob=");
    getEobAddress().describe(File);
  }
  fputs(">", File);
  return File;
}

}  // end of namespace decode

}  // end of namespace wasm
