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

#include "stream/PageCursor.h"

namespace wasm {

namespace decode {

BlockEob::BlockEob(AddressType Address) : EobAddress(Address) {
  init();
}

BlockEob::BlockEob(AddressType ByteAddr,
                   const std::shared_ptr<BlockEob> EnclosingEobPtr)
    : EobAddress(ByteAddr), EnclosingEobPtr(EnclosingEobPtr) {
  init();
}

BlockEob::~BlockEob() {
}

void BlockEob::fail() {
  BlockEob* Next = this;
  while (Next) {
    resetAddress(Next->EobAddress);
    Next = Next->EnclosingEobPtr.get();
  }
}

FILE* BlockEob::describe(FILE* File) const {
  fprintf(File, "eob=");
  describeAddress(File, EobAddress);
  return File;
}

Queue::Queue()
    : MinPeekSize(32),
      EofFrozen(false),
      Status(StatusValue::Good),
      EofPtr(std::make_shared<BlockEob>()) {
  // Verify we have space for kErrorPageAddress and kUndefinedAddress.
  assert(PageSizeLog2 > 1);
  LastPage = FirstPage = std::make_shared<Page>(0);
  PageMap.push_back(LastPage);
}

void Queue::close() {
  if (!isEofFrozen()) {
    AddressType EofAddress = LastPage->getMaxAddress();
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
  for (AddressType i = 0; i < PageMap.size(); ++i) {
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

std::shared_ptr<Page> Queue::getReadPage(AddressType& Address) const {
  AddressType Index = PageIndex(Address);
  if (Index >= PageMap.size())
    return const_cast<Queue*>(this)->readFillToPage(Index, Address);
  return getDefinedPage(Index, Address);
}

std::shared_ptr<Page> Queue::getWritePage(AddressType& Address) const {
  AddressType Index = PageIndex(Address);
  if (Index >= PageMap.size())
    return const_cast<Queue*>(this)->writeFillToPage(Index, Address);
  return getDefinedPage(Index, Address);
}

std::shared_ptr<Page> Queue::getCachedPage(AddressType& Address) {
  AddressType Index = PageIndex(Address);
  if (Index >= PageMap.size())
    return failThenGetErrorPage(Address);
  return getDefinedPage(Index, Address);
}

std::shared_ptr<Page> Queue::getDefinedPage(AddressType Index,
                                            AddressType& Address) const {
  assert(Index < PageMap.size());
  std::shared_ptr<Page> Pg = PageMap[Index].lock();
  if (Pg)
    return Pg;
  return const_cast<Queue*>(this)->failThenGetErrorPage(Address);
}

std::shared_ptr<Page> Queue::failThenGetErrorPage(AddressType& Address) {
  fail();
  Address = kErrorPageAddress;
  return getErrorPage();
}

bool Queue::appendPage() {
  AddressType NewPageIndex = LastPage->getPageIndex() + 1;
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

bool Queue::readFill(AddressType Address) {
  return Address < LastPage->getMaxAddress();
}

bool Queue::writeFill(AddressType Address, AddressType WantedSize) {
  Address += WantedSize;
  // Expand till page exists.
  while (Address > LastPage->getMaxAddress()) {
    if (EofFrozen)
      return false;
    AddressType MaxLimit = LastPage->getMinAddress() + PageSize;
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

std::shared_ptr<Page> Queue::readFillToPage(AddressType Index, AddressType& Address) {
  while (Index > LastPage->Index) {
    bool ReadFillNextPage = readFill(LastPage->getMinAddress() + PageSize);
    if (!ReadFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if ((LastPage->getMinAddress() + PageSize != LastPage->getMaxAddress()) ||
          !appendPage())
        return failThenGetErrorPage(Address);
    }
  }
  return getDefinedPage(Index, Address);
}

std::shared_ptr<Page> Queue::writeFillToPage(AddressType Index, AddressType& Address) {
  while (Index > LastPage->Index) {
    bool WriteFillNextPage = writeFill(LastPage->getMinAddress(), PageSize);
    if (!WriteFillNextPage && Index > LastPage->Index) {
      // This should only happen if we reach eof. Verify,
      // If so, allow page wrap so that we can have a cursor pointing
      // to the eof position.
      if ((LastPage->getMinAddress() + PageSize != LastPage->getMaxAddress()) ||
          !appendPage())
        return failThenGetErrorPage(Address);
    }
  }
  return getDefinedPage(Index, Address);
}

AddressType Queue::readFromPage(AddressType& Address,
                           AddressType WantedSize,
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

AddressType Queue::writeToPage(AddressType& Address,
                          AddressType WantedSize,
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

void Queue::freezeEof(AddressType& Address) {
  assert(Address <= kMaxEofAddress && "WASM stream too big to process");
  if (EofFrozen && Address != EofPtr->getEobAddress()) {
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

bool Queue::isBroken(const PageCursor& C) const {
  assert(C.CurPage);
  return C.CurPage->getPageIndex() >= kErrorPageIndex;
}

AddressType Queue::read(AddressType& Address, uint8_t* ToBuf, AddressType WantedSize) {
  AddressType Count = 0;
  PageCursor Cursor(this);
  while (WantedSize) {
    AddressType FoundSize = readFromPage(Address, WantedSize, Cursor);
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

bool Queue::write(AddressType& Address, uint8_t* FromBuf, AddressType WantedSize) {
  PageCursor Cursor(this);
  while (WantedSize) {
    AddressType FoundSize = writeToPage(Address, WantedSize, Cursor);
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
