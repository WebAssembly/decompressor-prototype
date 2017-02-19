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

#ifndef DECOMPRESSOR_SRC_STREAM_BLOCKEOB_H_
#define DECOMPRESSOR_SRC_STREAM_BLOCKEOB_H_

#include "stream/Address.h"

namespace wasm {

namespace decode {

class Page;
class PageCursor;

// Holds the end of a block within a queue. The outermost block is
// always defined as enclosing the entire queue. Note: EobBitAddress
// is the extra bits in the next byte, efter EobAddress, when eob doesn't
// align on a byte boundary (i.e. only applies to bit streams).
class BlockEob : public std::enable_shared_from_this<BlockEob> {
  BlockEob(const BlockEob&) = delete;
  BlockEob& operator=(const BlockEob&) = delete;

 public:
  explicit BlockEob(AddressType Address = kMaxEofAddress);
  BlockEob(AddressType ByteAddr,
           const std::shared_ptr<BlockEob> EnclosingEobPtr);
  ~BlockEob();
  AddressType& getEobAddress() { return EobAddress; }
  void setEobAddress(const AddressType& Address) { EobAddress = Address; }
  bool isGood() const { return isGoodAddress(EobAddress); }
  bool isDefined() const { return isDefinedAddress(EobAddress); }
  std::shared_ptr<BlockEob> getEnclosingEobPtr() const {
    return EnclosingEobPtr;
  }
  void fail();
  // For debugging.
  FILE* describe(FILE* File) const;

 private:
  AddressType EobAddress;
  std::shared_ptr<BlockEob> EnclosingEobPtr;
  void init() { assert(isGood()); }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_BLOCKEOB_H_
