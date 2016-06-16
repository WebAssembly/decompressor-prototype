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

// Defines a byte queue for hold bit and byte streams.
//
// Associated with the byte queue are locks.  This locking structure allows one
// to lock "backpatch" addresses, making sure that the pages are not thrown away
// until all locks have been released.  Locks will also block reads, until the
// locks are unlocked.
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
//
// TODO(KarlSchimpf): Make reads blocked if after a frozen page.
//
// TODO(KarlSchimpf): bocking not implemented. Currently, reads fail if they try
// to read from a locked page.

#ifndef DECOMPRESSOR_SRC_STREAM_QUEUE_H
#define DECOMPRESSOR_SRC_STREAM_QUEUE_H

#include "stream/RawStream.h"

#include <memory>
#include <queue>
#include <vector>

namespace wasm {

namespace decode {

class QueuePage;

class ByteQueue {
  ByteQueue(const ByteQueue &) = delete;
  ByteQueue &operator=(const ByteQueue &) = delete;
public:
  ByteQueue();

  virtual ~ByteQueue();

  // Returns the current size of the buffer.
  size_t size() const;

  // Defines the maximum Peek size into the queue when reading. That
  // is, The minimal number of bytes that the reader can back up without
  // freezing an address. Defaults to 32.
  void setMinPeekSize(size_t NewValue) { MinPeekSize = NewValue; }

  // Reads a contiguous range of elements into a buffer.
  //
  // Note: A read request may not be fully met. This function only guarantees
  // to read 1 element from the queue, if eob hasn't been reached. This is
  // done to minimize blocking. When possible, it will try to meet the full
  // request.
  //
  // TODO(KarlSchimpf): Add lock blocking if elements are not yet available.
  // Rather, than blocking, the current implementation returns 0.
  //
  // @param Address The addres within the queue to read from. Automatically
  //                incremented during read.
  // @param Buffer  A pointer to a buffer to be filled (and contains at least
  //                Size elements).
  // @param Size    The number of requested elements to read.
  // @result        The actual number of elements read. If zero, the eob was
  //                reached. Valid return values are in [0..Size].
  size_t read(size_t &Address, uint8_t *Buffer, size_t Size=1);

  // Writes a contiquous sequence of elements in the given buffer.
  //
  // Note: If Address is larger than queue size, zero's are automatically
  // inserted.
  //
  // @param Address    The addres within the queue to write to. Automatically
  //                   incremented during write.
  // @param Buffer     A pointer to the buffer of elements to write.
  // @param Size       The number of elements in the buffer to write.
  // @result           True if successful (i.e. not beyond eob address).
  bool write(size_t &Address, uint8_t *Buffer, size_t Size=1);

  // The following two methods allows one to lock the memory of the queue
  // directly, and read/write directly into the buffer. Also locks the
  // address, which must be released with a call to unlockAddress().
  //
  // WARNING: Never access beyond Size elements after the call, and
  // only while you haven't unlocked the address.  There is no
  // quarantee that such pointer accesses are vaild. It is left to an
  // implementation detail on how much can be locked down at one time.

  // Returns a pointer into the queue that can be read. Must unlock the address
  // once reading has been completed.
  //
  // TODO: bocking not implemented. Currently, reads fail if they try to read from
  // a locked page.
  //
  // @param Address    The address within the queue to Access.
  // @param WantedSize The number of bytess requested to read lock.
  //                   Note: Zero is allowed
  // @param LockedSize The actual number of bytes locked down (may be smaller
  //                   than wanted size, due to eob or internal paging).
  // @result           Pointer to locked address, or nullptr if unable to read
  //                   lock.
  uint8_t *getReadLockedPointer(size_t Address, size_t WantedSize,
                                size_t &LockedSize);

  // Returns a pointer into the queue that can be written to. Must unlock the
  // address once writing has been completed.
  //
  // Note: Assumes that caller will fill the wanted size. If not, call
  // freezeEob() to reset to the right size.

  // @param Address    The address within the queue to Access.
  // @param WantedSize The number of bytess requested to write lock.
  //                   Note: Zero is allowed
  // @param LockedSize The actual number of bytes locked down (may be smaller
  //                   than wanted size, due to eob or internal paging).
  // @result           Pointer to locked address, or nullptr if unable to write
  //                   lock.
  uint8_t *getWriteLockedPointer(size_t Address, size_t WantedSize,
                                 size_t &LockedSize);

  // Freezes eob of the queue. Not valid to read/write past the eob, once set.
  void freezeEob(size_t Address);

  bool isEobFrozen() const { return EobFrozen; }

  // Returns true if Address is locked.
  bool isAddressLocked(size_t Address) const;

  // Decrements the lock count for Address by 1. Assumes lock was
  // defined by previous (successful) calls to getReadLockedPointer()
  // and getWriteLockedPointer().
  void unlockAddress(size_t Address);

protected:
  // True if end of buffer has been frozen.
  bool EobFrozen = false;
  // First page still in queue.
  QueuePage *FirstPage;
  // Page at the current end of buffer.
  QueuePage *EobPage;
  // Minimum peek size to maintain. That is, the minimal number of
  // bytes that the read can back up without freezing an address.
  size_t MinPeekSize = 32;
  // Fast page lookup map (from page index)
  using PageMapType = std::vector<QueuePage*>;
  PageMapType PageMap;

  // Heap to keep track of pages locks, sorted by page index.
  std::priority_queue<size_t, std::vector<size_t>,
                      std::less<size_t>> LockedPages;

  // Returns the page in the queue referred to Address, or nullptr if no
  // such page is in the byte queue.
  QueuePage *getPage(size_t Address) const;

  // Returns the page with the given PageIndex, or nullptr if no such
  // page is in the byte queue.
  QueuePage *getPageAt(size_t PageIndex) const;

  // Increments the lock count on the given page.
  void lockPage(QueuePage *Page);

  // Decrements the lock count on the given page.
  void unlockPage(QueuePage *Page);

  // Fills buffer until we can read 1 or more bytes at the given address.
  // Returns true if successful.
  virtual bool readFill(size_t Address);

  // Dumps and deletes the first page.  Note: Dumping only occurs if a
  // Writer is provided (see class WriteBackedByteQueue below).
  virtual void dumpFirstPage();
};

// Read-only queue that is write-filled from a steam using the given
// Reader.
class ReadBackedByteQueue final : public ByteQueue {
  ReadBackedByteQueue(const ReadBackedByteQueue &) = delete;
  ReadBackedByteQueue &operator=(const ReadBackedByteQueue &) = delete;
  ReadBackedByteQueue() = delete;
public:
  ReadBackedByteQueue(std::unique_ptr<RawStream> Reader)
      : Reader(std::move(Reader)) {
    assert(Reader);
  }
  ~ReadBackedByteQueue() override {}
private:
  // Reader to write fill buffer as needed.
  std::unique_ptr<RawStream> Reader;

  bool readFill(size_t Address) override;
};

// Write-only queue that is dumped to a stream using the given Writer.
class WriteBackedByteQueue final : public ByteQueue {
  WriteBackedByteQueue(const WriteBackedByteQueue &) = delete;
  WriteBackedByteQueue &operator=(const WriteBackedByteQueue &) = delete;
  WriteBackedByteQueue() = delete;
public:
  WriteBackedByteQueue(std::unique_ptr<RawStream> Writer)
      : Writer(std::move(Writer)) {
    assert(Writer);
  }
  ~WriteBackedByteQueue() override {}
private:
  // Writer to dump contents of queue, when the contents is no longer
  // needed by reader.
  std::unique_ptr<RawStream> Writer;

  void dumpFirstPage() override;
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_QUEUE_H
