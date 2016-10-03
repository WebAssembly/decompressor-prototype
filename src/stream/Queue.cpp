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

#include <algorithm>

namespace wasm {

namespace decode {

FILE* BitAddress::describe(FILE* File) const {
  fprintf(File, "@%" PRIxMAX, ByteAddr);
  if (BitAddr != 0)
    fprintf(File, ":%u", BitAddr);
  return File;
}

void BlockEob::fail() {
  BlockEob* Next = this;
  while (Next) {
    Next->EobAddress.reset();
    Next = Next->EnclosingEobPtr.get();
  }
}

FILE* BlockEob::describe(FILE* File) const {
  fprintf(File, "eob=");
  return EobAddress.describe(File);
}

Queue::Queue()
    : MinPeekSize(32),
      EofFrozen(false),
      Status(StatusValue::Good),
      EofPtr(std::make_shared<BlockEob>()) {
  // Verify we have space for kErrorPageAddress and kUndefinedAddress.
  assert(Page::SizeLog2 > 1);
  LastPage = FirstPage = std::make_shared<Page>(0);
  PageMap.push_back(LastPage);
}

void Queue::close() {
  if (!isEofFrozen()) {
    size_t EofAddress = LastPage->getMaxAddress();
    freezeEof(EofAddress);
  }
  while (FirstPage)
    dumpFirstPage();
}

Queue::~Queue() {
  close();
}

void Queue::describe(FILE* Out) {
  fprintf(Out, "**** Queue %p ***\n", (void*)this);
  fprintf(Out, "First = %p, Last = %p\n", (void*)FirstPage.get(),
          (void*)LastPage.get());
  for (size_t i = 0; i < PageMap.size(); ++i) {
    std::shared_ptr<Page> Pg = PageMap[i].lock();
    if (Pg)
      Pg->describe(Out);
    else
      fprintf(Out, "Page[%" PRIuMAX "] = nullptr", uintmax_t(i));
    fprintf(Out, "\n");
  }
  if (ErrorPage) {
    fputs("Error ", Out);
    ErrorPage->describe(Out);
  }
  fprintf(Out, "*****************\n");
}

void Queue::fail() {
  Status = StatusValue::Bad;
  EofPtr->setEobAddress(0);
}

std::shared_ptr<Page> Queue::getErrorPage() {
  if (ErrorPage)
    return ErrorPage;
  ErrorPage = std::make_shared<Page>(kErrorPageIndex);
  return ErrorPage;
}

std::shared_ptr<Page> Queue::failThenGetErrorPage(size_t& Address) {
  fail();
  Address = kErrorPageAddress;
  return getErrorPage();
}

bool Queue::appendPage() {
  size_t NewPageIndex = LastPage->getPageIndex() + 1;
  if (NewPageIndex > kMaxPageIndex)
    return false;
  std::shared_ptr<Page> NewPage = std::make_shared<Page>(NewPageIndex);
  PageMap.push_back(NewPage);
  LastPage->Next = NewPage;
  LastPage = NewPage;
  return true;
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

bool Queue::writeFill(size_t Address, size_t WantedSize) {
  Address += WantedSize;
  // Expand till page exists.
  while (Address > LastPage->getMaxAddress()) {
    if (EofFrozen)
      return false;
    size_t MaxLimit = LastPage->getMinAddress() + Page::Size;
    if (Address >= MaxLimit) {
      LastPage->setMaxAddress(MaxLimit);
      if (!appendPage())
        return false;
    } else {
      LastPage->setMaxAddress(Address);
    }
  }
  return true;
}

std::shared_ptr<Page> Queue::readFillToPage(size_t Index, size_t& Address) {
  while (Index > LastPage->Index) {
    bool ReadFillNextPage = readFill(LastPage->getMinAddress() + Page::Size);
    if (!ReadFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if ((LastPage->getMinAddress() + Page::Size !=
           LastPage->getMaxAddress()) ||
          !appendPage())
        return failThenGetErrorPage(Address);
    }
  }
  return getDefinedPage(Index, Address);
}

std::shared_ptr<Page> Queue::writeFillToPage(size_t Index, size_t& Address) {
  while (Index > LastPage->Index) {
    bool WriteFillNextPage = writeFill(LastPage->getMinAddress(), Page::Size);
    if (!WriteFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if ((LastPage->getMinAddress() + Page::Size !=
           LastPage->getMaxAddress()) ||
          !appendPage())
        return failThenGetErrorPage(Address);
    }
  }
  return getDefinedPage(Index, Address);
}

size_t Queue::readFromPage(size_t& Address,
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

size_t Queue::writeToPage(size_t& Address,
                          size_t WantedSize,
                          PageCursor& Cursor) {
  // Expand till page exists.
  if (!writeFill(Address, WantedSize))
    return 0;
  Cursor.CurPage = getCachedPage(Address);
  Cursor.setCurAddress(Address);
  dumpPreviousPages();
  // Compute largest contiguous range of bytes available.
  if (Address + WantedSize > Cursor.getMaxAddress())
    WantedSize = Cursor.getMaxAddress() - Address;
  return WantedSize;
}

void Queue::freezeEof(size_t& Address) {
  assert(Address <= kMaxEofAddress && "WASM stream too big to process");
  if (EofFrozen && Address != EofPtr->getEobAddress().getByteAddress()) {
    fail();
    Address = 0;
  }
  // This call zero-fills pages if writing hasn't reached Address yet.
  PageCursor Cursor(this);
  writeToPage(Address, 0, Cursor);
  EofPtr->setEobAddress(Address);
  EofFrozen = true;
  if (!isBroken(Cursor)) {
    Cursor.setMaxAddress(Address);
    // TODO(karlschimpf): If adding threads, make this update thread safe.
    // If any pages exist after Cursor, remove them.
    LastPage = Cursor.CurPage;
    if (Cursor.CurPage->Next)
      Cursor.CurPage->Next.reset();
  }
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
