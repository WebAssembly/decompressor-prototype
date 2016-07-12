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

// Defines a queue for hold buffers to streams.
//
// Associated with queues are locks.  This locking structure allows one to lock
// "backpatch" addresses, making sure that the pages are not thrown away until
// all locks have been released.
//
// Note: Virtual addresses are used, start at index 0, and correspond to a
// buffer index as if the queue keeps all pages (i.e. doesn't shrink) until the
// queue is destructed. Therefore, if a byte is written at address N, to read
// the value you must always use address N.
//
// It is assumed that jumping on reads and writes are valid. However, back jumps
// are only safe if you lock the address before reading past that address.
//
// TODO(KarlSchimpf): Locking of reads/writes for threading has not yet been
// addressed, and the current implementation is NOT thread safe.

#ifndef DECOMPRESSOR_SRC_STREAM_QUEUE_H
#define DECOMPRESSOR_SRC_STREAM_QUEUE_H

#include "stream/Page.h"

#include <memory>
#include <queue>
#include <vector>

namespace wasm {

namespace decode {

template<class Base>
class Queue {
  Queue(const Queue &) = delete;
  Queue &operator=(const Queue &) = delete;

public:
  Queue();

  virtual ~Queue();

  // Defines the maximum Peek size into the queue when reading. That
  // is, The minimal number of bytes that the reader can back up without
  // freezing an address. Defaults to 32.
  void setMinPeekSize(size_t NewValue) {
    MinPeekSize = NewValue * sizeof(Base);
  }

  // Value unknown (returning maximum possible size) until frozen. When
  // frozen, returns the size of the buffer.
  size_t currentSize() {
    return EobFrozen
        ? EobPage->getMaxAddress() : std::numeric_limits<ssize_t>::max();
  }

  // Returns the actual size of the buffer (i.e. only those with pages still
  // in memory).
  size_t actualSize() const {
    return EobPage->getMaxAddress() - FirstPage->getMinAddress();
  }

  // Returns true if Address is locked.
  bool isAddressLocked(size_t Address) const {
    Page *P = getPage(Address);
    if (P == nullptr)
      return false;
    return P->isLocked();
  }

  // Increments the lock count for Address by 1. Assumes lock is already locked
  // (and hence defined).
  void lock(size_t Address) { lockPage(getPage(Address)); }

  // Decrements the lock count for Address by 1. Assumes lock was
  // defined by previous (successful) calls to getReadLockedPointer()
  // and getWriteLockedPointer().
  void unlock(size_t Address) { unlockPage(getPage(Address)); }


  // The following two methods allows one to lock the memory of the queue
  // directly, and read/write directly into the buffer. Also locks the
  // address, which must be released with a call to unlockAddress().
  //
  // WARNING: Never access beyond LockedSize elements after the call,
  // and only while you haven't unlocked the address.  There is no
  // guarantee that such pointer accesses are vaild, once it has been
  // unlocked.

  // Returns a pointer into the queue that can be read. Must unlock the address
  // once reading has been completed.
  //
  // @param Address    The address within the queue to Access.
  // @param WantedSize The number of elements requested to be read
  //                   locked.  Note: Zero is allowed.
  // @param LockedSize The actual number of elements locked down (may
  //                   be smaller than wanted size, due to eob or
  //                   internal paging).
  // @result           Pointer to locked address, or nullptr if unable to read
  //                   lock.
  Base *getReadLockedPointer(size_t Address, size_t WantedSize,
                             size_t &LockedSize);

  // Returns a pointer into the queue that can be written to. Must unlock the
  // address once writing has been completed.
  //
  // @param Address    The address within the queue to Access.
  // @param WantedSize The number of elements requested to bewrite
  //                   locked.  Note: Zero is allowed, and just locks
  //                   address.
  // @param LockedSize The actual number of elements locked down (may
  //                   be smaller than wanted size, due to eob or
  //                   internal paging).
  // @result           Pointer to locked address, or nullptr if unable to write
  //                   lock.
  Base *getWriteLockedPointer(size_t Address, size_t WantedSize,
                              size_t &LockedSize);

  // Freezes eob of the queue. Not valid to read/write past the eob, once set.
  void freezeEob(size_t Address);

  bool isEobFrozen() const { return EobFrozen; }

protected:
  // Minimum peek size to maintain. That is, the minimal number of
  // bytes that the read can back up without freezing an address.
  size_t MinPeekSize = 32 * sizeof(Base);
  // True if end of queue buffer has been frozen.
  bool EobFrozen = false;
  // First page still in queue.
  Page *FirstPage;
  // Page at the current end of buffer.
  Page *EobPage;
  // Fast page lookup map (from page index)
  using PageMapType = std::vector<Page*>;
  PageMapType PageMap;
  // Heap to keep track of pages locks, sorted by page index.
  std::priority_queue<size_t, std::vector<size_t>,
                      std::less<size_t>> LockedPages;

  // Returns the page in the queue referred to Address, or nullptr if no
  // such page is in the byte queue.
  Page *getPage(size_t Address) const {
    return getPageAt(Page::index(Address));
  }

  // Returns the page with the given PageIndex, or nullptr if no such
  // page is in the byte queue.
  Page *getPageAt(size_t PageIndex) const {
    return (PageIndex >= PageMap.size()) ? nullptr : PageMap[PageIndex];
  }

  // Increments the lock count on the given page.
  void lockPage(Page *P) {
    P->lock();
    LockedPages.emplace(P->Index);
  }

  // Decrements the lock count on the given page.
  void unlockPage(Page *P) {
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

  // Dumps and deletes the first page.  Note: Dumping only occurs if a
  // Writer is provided (see class WriteBackedByteQueue below).
  virtual void dumpFirstPage();

  // Dumps ununsed pages before the given address to recover memory.
  void dumpPreviousPages(size_t Address);

  // Fills buffer until we can read 1 or more bytes at the given address.
  // Returns true if successful.
  virtual bool readFill(size_t Address);
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_QUEUE_H
