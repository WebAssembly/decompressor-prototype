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

#include "stream/ByteQueue.h"
#include "stream/WriteUtils.h"

namespace wasm {

namespace decode {

void ByteQueue::writePageAt(FILE *File, size_t Address) {
  Page *P = getPage(Address);
  if (P == nullptr)
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

ByteQueue::ByteQueue() {
  EobPage = FirstPage = new Page(0);
  PageMap.push_back(EobPage);
}

ByteQueue::~ByteQueue() {
  while (FirstPage)
    dumpFirstPage();
}

size_t ByteQueue::size() const {
  return EobPage->getMaxAddress() - FirstPage->getMinAddress();
}

size_t ByteQueue::read(size_t &Address, uint8_t *ToBuf, size_t WantedSize) {
  size_t Count = 0;
  while (WantedSize) {
    size_t FoundSize;
    uint8_t *FromBuf = getReadLockedPointer(Address, WantedSize,
                                            FoundSize);
    if (FromBuf == nullptr)
      // TODO: Block if Count == 0 and blocked by a lock.
      return Count;
    memcpy(ToBuf, FromBuf, FoundSize);
    unlock(Address);
    Count += FoundSize;
    WantedSize -= FoundSize;
    Address += FoundSize;
    if (EobFrozen && Address >= EobPage->getMaxAddress())
      return EobPage->getMaxAddress();
  }
  return Count;
}

bool ByteQueue::write(size_t &Address, uint8_t *FromBuf, size_t WantedSize) {
  while (WantedSize) {
    size_t FoundSize;
    uint8_t *ToBuf = getWriteLockedPointer(Address, WantedSize, FoundSize);
    if (FromBuf == nullptr)
      return false;
    memcpy(ToBuf, FromBuf, FoundSize);
    unlock(Address);
    Address += FoundSize;
    WantedSize -= FoundSize;
  }
  return true;
}

void ByteQueue::dumpPreviousPages(size_t Address) {
  Page *AddrPage = getPage(Address);
  while (FirstPage != AddrPage) {
    if (!FirstPage->isLocked())
      break;
    if (FirstPage->getMaxAddress() + MinPeekSize < Address)
      break;
    dumpFirstPage();
  }
}

uint8_t *ByteQueue::getReadLockedPointer(size_t Address, size_t WantedSize,
                                         size_t &LockedSize) {
  // Start by read-filling if necessary.
  if (Address >= EobPage->getMaxAddress()) {
    if (!readFill(Address)) {
      LockedSize = 0;
      return nullptr;
    }
  }
  // Find page associated with Address.
  Page *ReadPage = getPage(Address);
  if (ReadPage == nullptr
      || (!LockedPages.empty() && ReadPage->Index < LockedPages.top())) {
    LockedSize = 0;
    return nullptr;
  }
  lock(ReadPage);
  dumpPreviousPages(Address);
  // Compute largest contiguous range of bytes available.
  LockedSize =
      (Address + WantedSize > ReadPage->getMaxAddress())
      ? ReadPage->getMaxAddress() - Address : WantedSize;
  return &ReadPage->Buffer[Page::address(Address)];
}

uint8_t *ByteQueue::getWriteLockedPointer(size_t Address, size_t WantedSize,
                                          size_t &LockedSize) {
  // Page doesn't exist. Expand queue if necessary.
  while (Address >= EobPage->getMaxAddress()) {
    if (EobFrozen) {
      LockedSize = 0;
      return nullptr;
    }
    EobPage->setMaxAddress(EobPage->getMinAddress() + Page::Size);
    if (Address < EobPage->getMaxAddress())
      break;
    Page *NewPage = new Page(EobPage->getMaxAddress());
    PageMap.push_back(NewPage);
    EobPage->Next = NewPage;
    NewPage->Last = EobPage;
    EobPage = NewPage;
  }
  Page *P = getPage(Address);
  if (P == nullptr) {
    LockedSize = 0;
    return nullptr;
  }
  lock(P);
  LockedSize = (Address + WantedSize > P->getMaxAddress())
      ? P->getMaxAddress() - Address : WantedSize;
  dumpPreviousPages(Address);
  return &P->Buffer[Page::address(Address)];
}

void ByteQueue::freezeEob(size_t Address) {
  assert(!EobFrozen);
  size_t LockedSize;
  // This call zero-fill pages if writing hasn't reached Address yet.
  assert(getWriteLockedPointer(Address, 0, LockedSize) != nullptr);
  Page *P = getPage(Address);
  P->setMaxAddress(Address);
  assert(P != nullptr);
  assert(P->Next == nullptr);
  unlock(Address);
}

bool ByteQueue::isAddressLocked(size_t Address) const {
  Page *P = getPage(Address);
  if (P == nullptr)
    return false;
  return P->isLocked();
}

Page *ByteQueue::getPageAt(size_t PageIndex) const {
  return (PageIndex >= PageMap.size()) ? nullptr : PageMap[PageIndex];
}

Page *ByteQueue::getPage(size_t Address) const {
  return getPageAt(Page::index(Address));
}

bool ByteQueue::readFill(size_t Address) {
  return Address < EobPage->getMaxAddress();
}

void ByteQueue::lock(Page *P) {
  P->lock();
  LockedPages.emplace(P->Index);
}

void ByteQueue::unlock(Page *P) {
  P->unlock();
  // Remove smallest page indices from queue that no longer have locks.
  while (!LockedPages.empty()) {
    size_t PageIndex = LockedPages.top();
    Page *P = getPageAt(PageIndex);
    if (P && P->isLocked())
      return;
    LockedPages.pop();
  }
}

void ByteQueue::dumpFirstPage() {
  Page *TmpPage = FirstPage;
  FirstPage = FirstPage->Next;
  delete TmpPage;
  if (FirstPage)
    FirstPage->Last = nullptr;
}

bool ReadBackedByteQueue::readFill(size_t Address) {
  if (Address < EobPage->getMaxAddress())
    return true;
  if (EobFrozen)
    return false;
  // Read fill if possible, until at least one byte is available.
  while (Address >= EobPage->getMaxAddress()) {
    if (size_t SpaceAvailable = EobPage->spaceRemaining()) {
      size_t NumBytes = Reader->read(
          &(EobPage->Buffer[Page::address(Address)]), SpaceAvailable);
      EobPage->incrementMaxAddress(NumBytes);
      if (NumBytes == 0) {
        EobFrozen = true;
        return false;
      }
      continue;
    }
    Page *NewPage = new Page(EobPage->getMaxAddress());
    PageMap.push_back(NewPage);
    EobPage->Next = NewPage;
    NewPage->Last = EobPage;
    EobPage = NewPage;
  }
  return true;
}

WriteBackedByteQueue::~WriteBackedByteQueue() {
  while (FirstPage)
    dumpFirstPage();
}

void WriteBackedByteQueue::dumpFirstPage() {
  size_t Address = 0;
  size_t Size = FirstPage->getMaxAddress() - FirstPage->getMinAddress();
  if (!Writer->write(&FirstPage->Buffer[Address], Size))
    fatal("Write to raw stream failed in ByteQueue::dumpFirstPage");
  ByteQueue::dumpFirstPage();
}

} // end of decode namespace

} // end of wasm namespace
