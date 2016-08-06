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
///

#include "stream/Queue.h"

namespace wasm {

namespace decode {

Queue::Queue()
    : MinPeekSize(32),
      EobFrozen(false),
      EofPtr(std::make_shared<BlockEob>()) {
  LastPage = FirstPage = std::make_shared<Page>(0);
  PageMap.push_back(LastPage);
}

Queue::~Queue() {
  while (FirstPage)
    dumpFirstPage();
}

void Queue::dumpFirstPage() {
  FirstPage = FirstPage->Next;
}

void Queue::dumpPreviousPages() {
  while (FirstPage.unique())
    dumpFirstPage();
}

bool Queue::readFill(size_t Address) {
  return Address < LastPage->getMaxAddress();
}

size_t Queue::readFromPage(size_t Address,
                           size_t WantedSize,
                           PageCursor& Cursor) {
  // Start by read-filling if necessary.
  if (Address >= LastPage->getMaxAddress()) {
    if (!readFill(Address))
      return 0;
  }
  // Find page associated with Address.
  Cursor.CurPage = getPage(Address);
  Cursor.setCurAddress(Address);
  if (!Cursor.CurPage)
    return 0;
  dumpPreviousPages();
  // Compute largest contiguous range of elements available.
  if (Address + WantedSize > Cursor.getMaxAddress())
    WantedSize = Cursor.getMaxAddress() - Address;
  return WantedSize;
}

size_t Queue::writeToPage(size_t Address,
                          size_t WantedSize,
                          PageCursor& Cursor) {
  // Expand till page exists.
  while (Address >= LastPage->getMaxAddress()) {
    if (EobFrozen)
      return 0;
    LastPage->setMaxAddress(LastPage->getMinAddress() + Page::Size);
    if (Address < LastPage->getMaxAddress())
      break;
    std::shared_ptr<Page> NewPage(
        std::make_shared<Page>(LastPage->getMaxAddress()));
    PageMap.push_back(NewPage);
    LastPage->Next = NewPage;
    LastPage = NewPage;
  }
  Cursor.CurPage = getPage(Address);
  Cursor.setCurAddress(Address);
  if (!Cursor.CurPage)
    return 0;
  dumpPreviousPages();
  // Compute largest contiguous range of bytes available.
  if (Address + WantedSize > Cursor.getMaxAddress())
    WantedSize = Cursor.getMaxAddress() - Address;
  return WantedSize;
}

void Queue::freezeEof(size_t Address) {
  assert(Address != kUndefinedAddress && "WASM stream too big to process");
  assert(!EobFrozen);
  // This call zero-fill pages if writing hasn't reached Address yet.
  PageCursor Cursor;
  writeToPage(Address, 0, Cursor);
  EofPtr->setEobAddress(Address);
  Cursor.setMaxAddress(Address);
  // TODO(karlschimpf): If adding threads, make this update thread safe.
  // If any pages exist after Cursor, remove them.
  LastPage = Cursor.CurPage;
  if (Cursor.CurPage->Next)
    Cursor.CurPage->Next.reset();
}

}  // end of decode namespace

}  // end of wasm namespace
