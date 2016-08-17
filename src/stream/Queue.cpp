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
#include "stream/WriteUtils.h"

namespace wasm {

namespace decode {

FILE* BitAddress::describe(FILE* File) const {
  fprintf(File, "@%" PRIxMAX, ByteAddr);
  if (BitAddr != 0)
    fprintf(File, ":%u", BitAddr);
  return File;
}

FILE* BlockEob::describe(FILE* File) const {
  fprintf(File, "eob");
  return EobAddress.describe(File);
}

Queue::Queue()
    : MinPeekSize(32), EofFrozen(false), EofPtr(std::make_shared<BlockEob>()) {
  LastPage = FirstPage = std::make_shared<Page>(0);
  PageMap.push_back(LastPage);
}

Queue::~Queue() {
  while (FirstPage)
    dumpFirstPage();
}

void Queue::describe(FILE* Out) {
  fprintf(Out, "**** Queue %p ***\n", (void*)this);
  fprintf(Out, "First = %p, Last = %p\n", (void*)FirstPage.get(), (void*)LastPage.get());
  for (size_t i = 0; i < PageMap.size(); ++i) {
    std::shared_ptr<Page> Pg = PageMap[i].lock();
    fprintf(Out, "[%" PRIuMAX "] = %p", uintmax_t(i), (void*)Pg.get());
    if (Pg) {
      fprintf(Out, " page %" PRIuMAX " [%" PRIxMAX "..%" PRIxMAX ") Next = %p",
              uintmax_t(Pg->getPageIndex()),
              uintmax_t(Pg->getMinAddress()), uintmax_t(Pg->getMaxAddress()),
              (void*)Pg->Next.get());
    }
    fprintf(Out, "\n");
  }
  fprintf(Out, "*****************\n");
}

void Queue::appendPage() {
  std::shared_ptr<Page> NewPage = std::make_shared<Page>(LastPage->getMaxAddress());
  PageMap.push_back(NewPage);
  LastPage->Next = NewPage;
  LastPage = NewPage;
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

bool Queue::writeFill(size_t Address) {
  // Expand till page exists.
  while (Address >= LastPage->getMaxAddress()) {
    if (EofFrozen)
      return false;
    LastPage->setMaxAddress(LastPage->getMinAddress() + Page::Size);
    if (Address < LastPage->getMaxAddress())
      return true;
    appendPage();
  }
  return true;
}

std::shared_ptr<Page> Queue::readFillToPage(size_t Index) {
  while (Index > LastPage->Index) {
    bool ReadFillNextPage =
        readFill(LastPage->getMinAddress() + Page::Size);
    if (!ReadFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if (LastPage->getMinAddress() + Page::Size != LastPage->getMaxAddress())
        fatal("readFillToPage failed!");
      appendPage();
    }
  }
  assert(Index < PageMap.size());
  return PageMap[Index].lock();
}

std::shared_ptr<Page> Queue::writeFillToPage(size_t Index) {
  while (Index > LastPage->Index) {
    bool WriteFillNextPage =
        writeFill(LastPage->getMinAddress() + Page::Size);
    if (!WriteFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if (LastPage->getMinAddress() + Page::Size != LastPage->getMaxAddress())
        fatal("readFillToPage failed!");
      appendPage();
    }
  }
  assert(Index < PageMap.size());
  return PageMap[Index].lock();
}

size_t Queue::readFromPage(size_t Address,
                           size_t WantedSize,
                           PageCursor& Cursor) {
  // Start by read-filling if necessary.
  if (Address >= LastPage->getMaxAddress() && !readFill(Address))
    return 0;
  // Find page associated with Address.
  Cursor.CurPage = getCachedPage(Address);
  Cursor.setCurAddress(Address);
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
  if (!writeFill(Address))
    return 0;
  Cursor.CurPage = getCachedPage(Address);
  Cursor.setCurAddress(Address);
  dumpPreviousPages();
  // Compute largest contiguous range of bytes available.
  if (Address + WantedSize > Cursor.getMaxAddress())
    WantedSize = Cursor.getMaxAddress() - Address;
  return WantedSize;
}

void Queue::freezeEof(size_t Address) {
  assert(Address != kUndefinedAddress && "WASM stream too big to process");
  assert(!EofFrozen);
  // This call zero-fill pages if writing hasn't reached Address yet.
  PageCursor Cursor(this);
  writeToPage(Address, 0, Cursor);
  EofPtr->setEobAddress(Address);
  Cursor.setMaxAddress(Address);
  // TODO(karlschimpf): If adding threads, make this update thread safe.
  // If any pages exist after Cursor, remove them.
  LastPage = Cursor.CurPage;
  if (Cursor.CurPage->Next)
    Cursor.CurPage->Next.reset();
  EofFrozen = true;
}

void Queue::writePageAt(FILE* File, size_t Address) {
  std::shared_ptr<Page> P = getWritePage(Address);
  if (!P)
    return;
  size_t Size = Page::address(Address);
  size_t Count = 0;
  for (size_t i = 0; i < Size; ++i, ++Count) {
    if (Count % 16 == 0) {
      if (Count)
        fputc('\n', File);
    } else {
      fputc(' ', File);
    }
    writeInt(File, P->Buffer[i], ValueFormat::Hexidecimal);
  }
  fputc('\n', File);
}

size_t Queue::read(size_t& Address, uint8_t* ToBuf, size_t WantedSize) {
  size_t Count = 0;
  PageCursor Cursor(this);
  while (WantedSize) {
    size_t FoundSize = readFromPage(Address, WantedSize, Cursor);
    if (FoundSize == 0)
      return Count;
    uint8_t* FromBuf = Cursor.getBufferPtr();
    memcpy(ToBuf, FromBuf, FoundSize);
    Count += FoundSize;
    WantedSize -= FoundSize;
    Address += FoundSize;
  }
  return Count;
}

bool Queue::write(size_t& Address, uint8_t* FromBuf, size_t WantedSize) {
  PageCursor Cursor(this);
  while (WantedSize) {
    size_t FoundSize = writeToPage(Address, WantedSize, Cursor);
    if (FoundSize == 0)
      return false;
    uint8_t* ToBuf = Cursor.getBufferPtr();
    memcpy(ToBuf, FromBuf, FoundSize);
    Address += FoundSize;
    WantedSize -= FoundSize;
  }
  return true;
}

}  // end of decode namespace

}  // end of wasm namespace
