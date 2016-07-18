/* -*- c++ -*- */
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

// Defines a generic base class for pages (of values). It is used to model
// streams in the WASM decompressor.
//
// Associated with pages are locks.  This locking structure allows one to lock
// "backpatch" addresses, making sure that the pages are not thrown away until
// all locks have been released.  Locks will also block reads, until the locks
// are unlocked.
//
// Note: Virtual addresses are used, start at index 0, and correspond to a
// buffer index as if the queue keeps all pages (i.e. doesn't shrink) until the
// queue is destructed. Therefore, if a byte is written at address N, to read
// the value you must always use address N.
//
// It is assumed that jumping on reads and writes are valid. However, back jumps
// are only safe if you lock the address before returning to that address.
//
// The memory for buffers are divided into pages. This is done so that the
// underlying data structure does not move once created, making threading a lot
// simplier to add. The size (i.e. number of bytes) in a page is a power of two,
// to that simple masking can be used to compute the page index and the byte
// address within the page.
//
// TODO(KarlSchimpf): Locking of reads/writes for threading has not yet been
// addressed, and the current implementation is NOT thread safe.
//
// TODO(KarlSchimpf): bocking not implemented. Currently, reads fail if they try
// to read from a locked page.

#ifndef DECOMPRESSOR_SRC_STREAM_PAGE_H
#define DECOMPRESSOR_SRC_STREAM_PAGE_H

#include "stream/RawStream.h"

#include <cstring>

namespace wasm {

namespace decode {

class Page {
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;

 public:
  static constexpr size_t SizeLog2 = 12;
  static constexpr size_t Size = 1 << SizeLog2;
  static constexpr size_t Mask = Size - 1;

  // Page index associated with address in queue.
  static constexpr size_t index(size_t Address) {
    return Address >> Page::SizeLog2;
  }

  // Returns address within a Page that refers to address.
  static constexpr size_t address(size_t Address) {
    return Address & Page::Mask;
  }

  Page(size_t MinAddress)
      : Index(Page::index(MinAddress)),
        MinAddress(MinAddress),
        MaxAddress(MinAddress) {
    std::memset(&Buffer, Page::Size, 0);
  }

  void lock() { ++LockCount; }

  void unlock() {
    assert(LockCount >= 1);
    --LockCount;
  }

  bool isLocked() const { return LockCount > 0; }

  size_t spaceRemaining() const {
    return (MinAddress + Page::Size == MaxAddress)
               ? 0
               : (Page::Size - (MaxAddress & Page::Mask));
  }

  size_t getMinAddress() const { return MinAddress; }

  size_t getMaxAddress() const { return MaxAddress; }

  void setMaxAddress(size_t NewValue) { MaxAddress = NewValue; }

  void incrementMaxAddress(size_t Increment = 1) { MaxAddress += Increment; }

  // The contents of the page.
  uint8_t Buffer[Page::Size];
  // The page index of the page.
  size_t Index;
  Page* Last = nullptr;
  Page* Next = nullptr;

 protected:
  // Note: Buffer address range is [MinAddress, MaxAddress).
  size_t MinAddress;
  size_t MaxAddress;
  size_t LockCount = 0;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_PAGE_H
