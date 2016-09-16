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

// Defines a queue for hold buffers to streams.
//
// Shared pointers to pages are used to effectively lock pages in the buffer.
// This allows one to "backpatch" addresses, making sure that the pages are not
// thrown away until all shared pointers have been released.
//
// Note: Virtual addresses are used, start at index 0, and correspond to a
// buffer index as if the queue keeps all pages (i.e. doesn't shrink) until the
// queue is destructed. Therefore, if a byte is written at address N, to read
// the value you must always use address N.
//
// It is assumed that jumping on reads and writes are valid. However, back jumps
// are only safe if you already have a cursor pointing to the page to be
// backpatched.
//
// TODO(KarlSchimpf): Locking of reads/writes for threading has not yet been
// addressed, and the current implementation is NOT thread safe.

#ifndef DECOMPRESSOR_SRC_STREAM_QUEUE_H
#define DECOMPRESSOR_SRC_STREAM_QUEUE_H

#include "stream/Page.h"

#include <memory>
#include <vector>

namespace wasm {

namespace decode {

// Note: We reserve the last page to be an "error" page. This allows us to
// guarantee that read/write cursors are always associated with a (defined)
// page.
static constexpr size_t kMaxEofAddress = ~size_t(0) << Page::SizeLog2;
static constexpr size_t kMaxPageIndex = Page::index(kMaxEofAddress);
static constexpr size_t kErrorPageAddress = kMaxEofAddress + 1;
static constexpr size_t kErrorPageIndex = Page::index(kErrorPageAddress);
static constexpr size_t kUndefinedAddress = std::numeric_limits<size_t>::max();

typedef uint8_t BitsInByteType;

struct BitAddress {
  friend class BlockEob;

 public:
  BitAddress(size_t ByteAddr = 0, BitsInByteType BitAddr = 0)
      : ByteAddr(ByteAddr), BitAddr(BitAddr) {}
  BitAddress(const BitAddress& Address)
      : ByteAddr(Address.ByteAddr), BitAddr(Address.BitAddr) {}
  size_t getByteAddress() const { return ByteAddr; }
  BitsInByteType getBitAddress() const { return BitAddr; }
  bool operator==(const BitAddress& Addr) {
    return ByteAddr == Addr.ByteAddr && BitAddr == Addr.BitAddr;
  }
  bool isGood() const { return ByteAddr <= kMaxEofAddress; }
  bool isDefined() const { return ByteAddr != kUndefinedAddress; }

  // For debugging
  FILE* describe(FILE* File) const;

 protected:
  size_t ByteAddr;
  BitsInByteType BitAddr;
  void reset() {
    ByteAddr = 0;
    BitAddr = 0;
  }
};

// Holds the end of a block within a queue. The outermost block is
// always defined as enclosing the entire queue. Note: EobBitAddress
// is the extra bits in the next byte, efter EobAddress, when eob doesn't
// align on a byte boundary (i.e. only applies to bit streams).
class BlockEob : public std::enable_shared_from_this<BlockEob> {
  BlockEob(const BlockEob&) = delete;
  BlockEob& operator=(const BlockEob&) = delete;

 public:
  explicit BlockEob(const BitAddress& Address) : EobAddress(Address) { init(); }
  explicit BlockEob(size_t ByteAddr = kMaxEofAddress,
                    BitsInByteType BitAddr = 0)
      : EobAddress(ByteAddr, BitAddr) {
    init();
  }
  BlockEob(size_t ByteAddr, const std::shared_ptr<BlockEob> EnclosingEobPtr)
      : EobAddress(ByteAddr), EnclosingEobPtr(EnclosingEobPtr) {
    init();
  }
  BlockEob(size_t ByteAddr,
           BitsInByteType BitAddr,
           const std::shared_ptr<BlockEob> EnclosingEobPtr)
      : EobAddress(ByteAddr, BitAddr), EnclosingEobPtr(EnclosingEobPtr) {
    init();
  }
  BlockEob(const BitAddress& Address,
           const std::shared_ptr<BlockEob> EnclosingEobPtr)
      : EobAddress(Address), EnclosingEobPtr(EnclosingEobPtr) {
    init();
  }
  BitAddress& getEobAddress() { return EobAddress; }
  void setEobAddress(const BitAddress& Address) { EobAddress = Address; }
  bool isGood() const { return EobAddress.isGood(); }
  bool isDefined() const { return EobAddress.isDefined(); }
  std::shared_ptr<BlockEob> getEnclosingEobPtr() const {
    return EnclosingEobPtr;
  }

  void fail();

  // For debugging.
  FILE* describe(FILE* File) const;

 private:
  BitAddress EobAddress;
  std::shared_ptr<BlockEob> EnclosingEobPtr;
  void init() { assert(isGood()); }
};

class Queue : public std::enable_shared_from_this<Queue> {
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;
  friend class Cursor;
  friend class PageCursor;
  friend class ReadCursor;
  friend class WriteCursor;

 public:
  enum class StatusValue { Good, Bad };
  Queue();

  virtual ~Queue();

  // Defines the maximum Peek size into the queue when reading. That
  // is, The minimal number of bytes that the reader can back up without
  // freezing an address. Defaults to 32.
  void setMinPeekSize(size_t NewValue) { MinPeekSize = NewValue; }

  // Value unknown (returning maximum possible size) until frozen. When
  // frozen, returns the size of the buffer.
  size_t currentSize() { return EofPtr->getEobAddress().getByteAddress(); }

  size_t fillSize() const { return LastPage->getMaxAddress(); }

  // Returns the actual size of the buffer (i.e. only those with pages still
  // in memory).
  size_t actualSize() const {
    return LastPage->getMaxAddress() - FirstPage->getMinAddress();
  }

  // Update Cursor to point to the given Address, and make (up to) WantedSize
  // elements available for reading. Returns the actual number of elements
  // available for reading. Note: Address will be moved to an error address
  // if an error occurs while reading.
  size_t readFromPage(size_t& Address, size_t WantedSize, PageCursor& Cursor);

  // Update Cursor to point to the given Address, and make (up to) WantedSize
  // elements available for writing. Returns the actual number of elements
  // available for writing. Note: Address will be moved to an error address
  // if an error occurs while writing.
  size_t writeToPage(size_t& Address, size_t WantedSize, PageCursor& Cursor);

  // Reads a contiguous range of bytes into a buffer.
  //
  // Note: A read request may not be fully met. This function only guarantees
  // to read 1 element from the queue, if eob hasn't been reached. This is
  // done to minimize blocking. When possible, it will try to meet the full
  // request.
  //
  // @param Address The address within the queue to read from. Automatically
  //                incremented during read.
  // @param Buffer  A pointer to a buffer to be filled (and contains at least
  //                Size elements).
  // @param Size    The number of requested elements to read.
  // @result        The actual number of elements read. If zero, the eob was
  //                reached. Valid return values are in [0..Size].
  size_t read(size_t& Address, uint8_t* Buffer, size_t Size = 1);

  // Writes a contiquous sequence of bytes in the given buffer.
  //
  // Note: If Address is larger than queue size, zero's are automatically
  // inserted.
  //
  // @param Address The address within the queue to write to. Automatically
  //                incremented during write.
  // @param Buffer  A pointer to the buffer of elements to write.
  // @param Size    The number of elements in the buffer to write.
  // @result        True if successful (i.e. not beyond eob address).
  bool write(size_t& Address, uint8_t* Buffer, size_t Size = 1);

  size_t getEofAddress() const {
    return EofPtr->getEobAddress().getByteAddress();
  }

  // Freezes eob of the queue. Not valid to read/write past the eob, once set.
  // Note: May change Address if queue is broken, or Address not valid.
  void freezeEof(size_t& Address);

  bool isBroken(const PageCursor& C) const {
    assert(C.CurPage);
    return C.CurPage->getPageIndex() >= kErrorPageIndex;
  }
  bool isEofFrozen() const { return EofFrozen; }
  bool isGood() const { return Status == StatusValue::Good; }

  const std::shared_ptr<BlockEob>& getEofPtr() const { return EofPtr; }

  // Mark queue as broken.
  void fail();

  void describe(FILE* Out);

 protected:
  // Minimum peek size to maintain. That is, the minimal number of
  // bytes that the read can back up without freezing an address.
  size_t MinPeekSize;
  // True if end of queue buffer has been frozen.
  bool EofFrozen;
  StatusValue Status;
  std::shared_ptr<BlockEob> EofPtr;
  // First page still in queue.
  std::shared_ptr<Page> FirstPage;
  // Page at the current end of buffer.
  std::shared_ptr<Page> LastPage;
  // Page to use if an error occurs.
  std::shared_ptr<Page> ErrorPage;
  // Fast page lookup map (from page index)
  typedef std::vector<std::weak_ptr<Page>> PageMapType;
  PageMapType PageMap;

  std::shared_ptr<Page> failThenGetErrorPage(size_t& Address);

  bool appendPage();

  std::shared_ptr<Page> getErrorPage();

  // Returns the page in the queue referred to Address, or nullptr if no
  // such page is in the byte queue.
  std::shared_ptr<Page> getReadPage(size_t& Address) const {
    size_t Index = Page::index(Address);
    if (Index >= PageMap.size())
      return const_cast<Queue*>(this)->readFillToPage(Index, Address);
    return getDefinedPage(Index, Address);
  }

  std::shared_ptr<Page> getWritePage(size_t& Address) const {
    size_t Index = Page::index(Address);
    if (Index >= PageMap.size())
      return const_cast<Queue*>(this)->writeFillToPage(Index, Address);
    return getDefinedPage(Index, Address);
  }

  std::shared_ptr<Page> getCachedPage(size_t& Address) {
    size_t Index = Page::index(Address);
    if (Index >= PageMap.size())
      return failThenGetErrorPage(Address);
    return getDefinedPage(Index, Address);
  }

  std::shared_ptr<Page> getDefinedPage(size_t Index, size_t& Address) const {
    assert(Index < PageMap.size());
    std::shared_ptr<Page> Pg = PageMap[Index].lock();
    if (Pg)
      return Pg;
    return const_cast<Queue*>(this)->failThenGetErrorPage(Address);
  }

  std::shared_ptr<Page> readFillToPage(size_t Index, size_t& Address);

  std::shared_ptr<Page> writeFillToPage(size_t Index, size_t& Address);

  bool isValidPageAddress(size_t Address) {
    return Page::index(Address) < PageMap.size();
  }

  // Dumps and deletes the first page.  Note: Dumping only occurs if a
  // Writer is provided (see class WriteBackedByteQueue below).
  virtual void dumpFirstPage();

  // Dumps ununsed pages prior to Address. If applicable, writes out bytes to
  // corresponding output stream before dumping.
  void dumpPreviousPages();

  // Fills buffer until we can read 1 or more bytes at the given address.
  // Returns true if successful. If applicable, reads from input to fill the
  // buffer as appropriate.
  virtual bool readFill(size_t Address);

  virtual bool writeFill(size_t Address);
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_QUEUE_H
