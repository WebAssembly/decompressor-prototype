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
// Only shared pointers are used with pages. This guarantees that the
// implementation of a queue (in streams/Queue.h) can figure out what
// pages are no longer used.
//
// Note: If you need to "backpatch" an address, be sure to store a
// shared pointer to that backpatch address, so that the page doesn't
// accidentally get garbage collected.
//
// Note: Virtual addresses are used, start at index 0, and correspond to a
// buffer index as if the queue keeps all pages (i.e. doesn't shrink) until the
// queue is destructed. Therefore, if a byte is written at address N, to read
// the value you must always use address N.
//
// It is assumed that jumping on reads and writes are valid. However,
// back jumps are only safe if you maintain a shared pointer to the
// address before returning to that address.
//
// The memory for buffers are divided into pages. This is done so that
// the underlying data structure does not move once created. This
// makes threading a lot simplier to add, since data does not
// move. The size (i.e. number of bytes) in a page is a power of two,
// to that simple masking can be used to compute the page index and
// the byte address within the page.
//
// TODO(karlschimpf) The current implementation is NOT thread safe, but should
// be easy to make thread safe.

#ifndef DECOMPRESSOR_SRC_STREAM_PAGE_H
#define DECOMPRESSOR_SRC_STREAM_PAGE_H

#include "stream/RawStream.h"

#include <cstring>
#include <memory>

namespace wasm {

namespace decode {

class Page : public std::enable_shared_from_this<Page> {
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;

 public:
  // Allow up to a megabyte per page.
  static constexpr size_t SizeLog2 = 16;
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
    std::memset(&Buffer, 0, Page::Size);
  }

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
  // Note: Buffer address range is [MinAddress, MaxAddress).
  size_t MinAddress;
  size_t MaxAddress;
  std::shared_ptr<Page> Next;

  // For debugging only.
  void describe(FILE* File) {
    fprintf(File, "Page[%" PRIuMAX "] [%" PRIuMAX ":%" PRIuMAX ") = %p\n",
            uintmax_t(Index), uintmax_t(MinAddress), uintmax_t(MaxAddress),
            (void*)this);
  }
};

class PageCursor {
 public:
  PageCursor() : CurAddress(0) {}
  PageCursor(std::shared_ptr<Page> CurPage, size_t CurAddress)
      : CurPage(CurPage), CurAddress(CurAddress) {}
  PageCursor(const PageCursor& PC)
      : CurPage(PC.CurPage), CurAddress(PC.CurAddress) {}
  size_t getMinAddress() const {
    return CurPage ? CurPage->getMinAddress() : 0;
  }
  PageCursor& operator=(const PageCursor& C) {
    CurPage = C.CurPage;
    CurAddress = C.CurAddress;
    return *this;
  }
  size_t getMaxAddress() const {
    return CurPage ? CurPage->getMaxAddress() : 0;
  }
  bool isValidPageAddress(size_t Address) {
    return getMinAddress() <= Address && Address < getMaxAddress();
  }
  void setCurAddress(size_t NewAddress) { CurAddress = NewAddress; }
  size_t getCurAddress() const { return CurAddress; }
  void setMaxAddress(size_t Address) { CurPage->setMaxAddress(Address); }
  bool isIndexAtEndOfPage() const { return getCurAddress() == getMaxAddress(); }
  uint8_t* getBufferPtr() {
    assert(CurPage);
    return CurPage->Buffer + getCurIndex();
  }
  uint8_t readByte() {
    uint8_t Byte = *getBufferPtr();
    ++CurAddress;
    return Byte;
  }
  void writeByte(uint8_t Byte) {
    *getBufferPtr() = Byte;
    ++CurAddress;
  }

  // TODO(karlschimpf): Make this private.
  std::shared_ptr<Page> CurPage;

  // For debugging only.
  void describe(FILE* File) {
    fprintf(File, "PC %" PRIuMAX ": ", uintmax_t(CurAddress));
    if (CurPage)
      CurPage->describe(File);
    else
      fprintf(File, " nullptr\n");
  }

 private:
  size_t CurAddress;  // Relative to minimum index in page.
  size_t getCurIndex() const { return CurAddress - CurPage->getMinAddress(); }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_PAGE_H
