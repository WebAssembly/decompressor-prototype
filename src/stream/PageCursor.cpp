// -*- c++ -*-
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

#include "stream/PageCursor.h"

#include "stream/Queue.h"

namespace wasm {

namespace decode {

#if 0
Page::Page(size_t PageIndex)
    : Index(PageIndex),
      MinAddress(minAddressForPage(PageIndex)),
      MaxAddress(minAddressForPage(PageIndex)) {
  std::memset(&Buffer, 0, Page::Size);
}

size_t Page::spaceRemaining() const {
  return MinAddress == MaxAddress
             ? Page::Size
             : Page::Size - (Page::address(MaxAddress - 1) + 1);
}

FILE* Page::describe(FILE* File) {
  fprintf(File, "Page[%" PRIuMAX "] [%" PRIxMAX "..%" PRIxMAX ") = %p",
          uintmax_t(Index), uintmax_t(MinAddress), uintmax_t(MaxAddress),
          (void*)this);
  return File;
}
#endif

PageCursor::PageCursor(Queue* Que)
    : CurPage(Que->FirstPage), CurAddress(Que->FirstPage->getMinAddress()) {
  assert(CurPage);
}

void describePage(FILE* File, Page* Pg) {
  if (Pg == nullptr)
    fprintf(File, " nullptr");
  else
    Pg->describe(File);
}

FILE* PageCursor::describe(FILE* File, bool IncludePage) {
  AddressType Addr(CurAddress);
  describeAddress(File, Addr);
  if (IncludePage)
    describePage(File, CurPage.get());
  return File;
}

}  // end of namespace decode

}  // end of namespace wasm
