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

#include "stream/Page.h"
#include "stream/Queue.h"

namespace wasm {

namespace decode {

Page::Page(AddressType PageIndex)
    : Index(PageIndex),
      MinAddress(minAddressForPage(PageIndex)),
      MaxAddress(minAddressForPage(PageIndex)) {
  std::memset(&Buffer, 0, PageSize);
}

AddressType Page::spaceRemaining() const {
  return MinAddress == MaxAddress
             ? PageSize
             : PageSize - (PageAddress(MaxAddress - 1) + 1);
}

FILE* Page::describe(FILE* File) {
  fprintf(File, "Page[%" PRIuMAX "] [%" PRIxMAX "..%" PRIxMAX ") = %p",
          uintmax_t(Index), uintmax_t(MinAddress), uintmax_t(MaxAddress),
          (void*)this);
  return File;
}

void describePage(FILE* File, Page* Pg) {
  if (Pg == nullptr)
    fprintf(File, " nullptr");
  else
    Pg->describe(File);
}

}  // end of namespace decode

}  // end of namespace wasm
