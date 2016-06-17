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

#include <cstring>

namespace wasm {

namespace decode {

namespace {

constexpr size_t BufferSizeLog2 = 12;
constexpr size_t BufferSize = 1 << BufferSizeLog2;
constexpr size_t BufferMask = BufferSize - 1;

// Page index associated with address in queue.
static constexpr size_t page(size_t Address) {
  return Address >> BufferSizeLog2;
}

// Returns address within a QueuePage that refers to address.
size_t pageAddress(size_t Address) {
  return Address | BufferMask;
}

} // end of anonymous namespace

class QueuePage {
  QueuePage(const QueuePage &) = delete;
  QueuePage &operator=(const QueuePage &) = delete;
public:
  QueuePage(size_t MinAddress)
      : PageIndex(page(MinAddress)), MinAddress(MinAddress),
        MaxAddress(MinAddress) {
    std::memset(&Buffer, BufferSize, 0);
  }

  void lockPage() { ++LockCount; }

  void unlockPage() {
    assert(LockCount > 1);
    --LockCount;
  }

  bool isLocked() const { return LockCount > 0; }

  size_t spaceRemaining() const {
    return BufferSize - (MaxAddress | BufferMask);
  }

  uint8_t Buffer[BufferSize];
  size_t PageIndex;
  // Note: Buffer address range is [MinAddress, MaxAddress).
  size_t MinAddress;
  size_t MaxAddress;
  size_t LockCount = 0;
  QueuePage *Last = nullptr;
  QueuePage *Next = nullptr;
};

ByteQueue::ByteQueue() {
  EobPage = FirstPage = new QueuePage(0);
  PageMap[0] = EobPage;
}

ByteQueue::~ByteQueue() {
  while (FirstPage)
    dumpFirstPage();
}

size_t ByteQueue::size() const {
  return EobPage->MaxAddress - FirstPage->MinAddress;
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
    unlockAddress(Address);
    Count += FoundSize;
    WantedSize -= FoundSize;
    Address += FoundSize;
    if (EobFrozen && Address >= EobPage->MaxAddress)
      return EobPage->MaxAddress;
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
    unlockAddress(Address);
    Address += FoundSize;
    WantedSize -= FoundSize;
  }
  return true;
}

uint8_t *ByteQueue::getReadLockedPointer(size_t Address, size_t WantedSize,
                                         size_t &LockedSize) {
  // Start by read-filling if necessary.
  if (Address >= EobPage->MaxAddress) {
    if (!readFill(Address)) {
      LockedSize = 0;
      return nullptr;
    }
  }
  // Find page associated with Address.
  QueuePage *ReadPage = getPage(Address);
  if (ReadPage == nullptr
      || (!LockedPages.empty() && ReadPage->PageIndex >= LockedPages.top())) {
    LockedSize = 0;
    return nullptr;
  }
  lockPage(ReadPage);
  // Free up pages no longer used.
  while (FirstPage != ReadPage) {
    if (!FirstPage->isLocked())
      break;
    if (FirstPage->MaxAddress + MinPeekSize < Address)
      break;
    dumpFirstPage();
  }
  // Compute largest contiguous range of bytes available.
  LockedSize =
      (Address + WantedSize > ReadPage->MaxAddress)
      ? ReadPage->MaxAddress - Address : WantedSize;
  return &ReadPage->Buffer[pageAddress(Address)];
}

uint8_t *ByteQueue::getWriteLockedPointer(size_t Address, size_t WantedSize,
                                          size_t &LockedSize) {
  // Page doesn't exist. Expand queue if necessary.
  while (Address >= EobPage->MaxAddress) {
    if (EobFrozen) {
      LockedSize = 0;
      return nullptr;
    }
    EobPage->MaxAddress = std::min(Address, EobPage->MinAddress + BufferSize);
    if (Address < EobPage->MaxAddress)
      break;
    QueuePage *Page = new QueuePage(EobPage->MaxAddress);
    PageMap[page(Page->MinAddress)] = Page;
    EobPage->Next = Page;
    Page->Last = EobPage;
    EobPage = Page;
  }
  QueuePage *Page = getPage(Address);
  if (Page == nullptr) {
    LockedSize = 0;
    return nullptr;
  }
  lockPage(Page);
  LockedSize =
      (Address + WantedSize > Page->MaxAddress)
      ? Page->MaxAddress - Address : WantedSize;
  return &Page->Buffer[pageAddress(Address)];
}

void ByteQueue::freezeEob(size_t Address) {
  assert(!EobFrozen);
  size_t LockedSize;
  // This call zero-fill pages if writing hasn't reached Address yet.
  assert(getWriteLockedPointer(Address, 0, LockedSize) != nullptr);
  QueuePage *Page = getPage(Address);
  assert(Page != nullptr);
  assert(Page->Next == nullptr);
  assert(Page->MaxAddress == Address);
}

bool ByteQueue::isAddressLocked(size_t Address) const {
  QueuePage *Page = getPage(Address);
  if (Page == nullptr)
    return false;
  return Page->isLocked();
}

void ByteQueue::unlockAddress(size_t Address) {
  QueuePage *Page = getPage(Address);
  assert(Page != nullptr);
  unlockPage(Page);
}

QueuePage *ByteQueue::getPageAt(size_t PageIndex) const {
  return (PageIndex >= PageMap.size()) ? nullptr : PageMap[PageIndex];
}

QueuePage *ByteQueue::getPage(size_t Address) const {
  return getPageAt(page(Address));
}

bool ByteQueue::readFill(size_t Address) {
  return Address < EobPage->MaxAddress;
}

void ByteQueue::lockPage(QueuePage *Page) {
  Page->lockPage();
  LockedPages.emplace(Page->PageIndex);
}

void ByteQueue::unlockPage(QueuePage *Page) {
  Page->unlockPage();
  // Remove smallest page indices from queue that no longer have locks.
  while (!LockedPages.empty()) {
    size_t PageIndex = LockedPages.top();
    QueuePage *Page = getPageAt(PageIndex);
    if (Page && Page->isLocked())
      return;
    LockedPages.pop();
  }
}

void ByteQueue::dumpFirstPage() {
  QueuePage *Tmp = FirstPage;
  FirstPage = FirstPage->Next;
  delete Tmp;
  if (FirstPage)
    FirstPage->Last = nullptr;
}

bool ReadBackedByteQueue::readFill(size_t Address) {
  if (Address < EobPage->MaxAddress)
    return true;
  if (EobFrozen)
    return false;
  // Read fill if possible, until at least one byte is available.
  while (Address >= EobPage->MaxAddress) {
    if (size_t SpaceAvailable = EobPage->spaceRemaining()) {
      size_t NumBytes = Reader->read(
          &EobPage->Buffer[pageAddress(Address)], SpaceAvailable);
      EobPage->MaxAddress += NumBytes;
      if (NumBytes == 0) {
        EobFrozen = true;
        return false;
      }
      continue;
    }
    QueuePage *Page = new QueuePage(EobPage->MaxAddress);
    PageMap[page(Page->MinAddress)] = Page;
    EobPage->Next = Page;
    Page->Last = EobPage;
    EobPage = Page;
  }
  return true;
}

void WriteBackedByteQueue::dumpFirstPage() {
  size_t Address = 0;
  size_t Size = FirstPage->MaxAddress - FirstPage->MinAddress;
  while (Size) {
    size_t NumBytes = Writer->write(&FirstPage->Buffer[Address], Size);
    if (NumBytes == 0)
      fatal("Write to raw stream failed in ByteQueue::dumpFirstPage");
    Address += NumBytes;
    Size -= NumBytes;
  }
  ByteQueue::dumpFirstPage();
}

} // end of decode namespace

} // end of wasm namespace
