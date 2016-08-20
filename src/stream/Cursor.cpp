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

void Cursor::fail() {
  Que->fail();
  CurPage = Que->getErrorPage();
  CurAddress = kErrorPageAddress;
  updateGuaranteedBeforeEob();
  EobPtr->fail();
}

bool Cursor::readFillBuffer() {
  if (CurAddress >= Que->getEofAddress())
    return false;
  size_t BufferSize = Que->readFromPage(CurAddress, Page::Size, *this);
  return BufferSize > 0;
}

void Cursor::writeFillBuffer() {
  if (CurAddress >= Que->getEofAddress()) {
    fail();
    return;
  }
  size_t BufferSize = Que->writeToPage(CurAddress, Page::Size, *this);
  if (BufferSize == 0)
    fail();
}

FILE* Cursor::describe(FILE* File, bool IncludeDetail, bool AddEoln) {
  if (IncludeDetail)
    fputs("Cursor<", File);
  PageCursor::describe(File, IncludeDetail);
  if (!CurByte.isEmpty()) {
    fputc(' ', File);
    CurByte.describe(File);
  }
  if (!IncludeDetail)
    return File;
  if (EobPtr->isDefined()) {
    fprintf(File, ", eob=");
    getEobAddress().describe(File);
  }
  fputc('>', File);
  if (AddEoln)
    fputc('\n', File);
  return File;
}

}  // end of namespace decode

}  // end of namespace wasm
