// -*- c++ -*-
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

#ifndef DECOMPRESSOR_SRC_STREAM_PAGE_H_
#define DECOMPRESSOR_SRC_STREAM_PAGE_H_

#include "stream/Address.h"
#include "stream/RawStream.h"

namespace wasm {

namespace decode {

class Queue;

class Page : public std::enable_shared_from_this<Page> {
  Page() = delete;
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  friend class Queue;

 public:

  Page(size_t PageIndex);
  size_t spaceRemaining() const;
  size_t getPageIndex() const { return Index; }
  size_t getMinAddress() const { return MinAddress; }
  size_t getMaxAddress() const { return MaxAddress; }
  size_t getPageSize() const { return MaxAddress - MinAddress; }
  void setMaxAddress(size_t NewValue) { MaxAddress = NewValue; }
  void incrementMaxAddress(size_t Increment = 1) { MaxAddress += Increment; }
  uint8_t getByte(size_t i) const { return Buffer[i]; }
  uint8_t* getByteAddress(size_t i) { return &Buffer[i]; }

  // For debugging only.
  FILE* describe(FILE* File);

 protected:
  // The contents of the page.
  uint8_t Buffer[PageSize];
  // The page index of the page.
  size_t Index;
  // Note: Buffer address range is [MinAddress, MaxAddress).
  size_t MinAddress;
  size_t MaxAddress;
  std::shared_ptr<Page> Next;
};

void describePage(FILE* File, Page* Pg);

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_PAGE_H_
