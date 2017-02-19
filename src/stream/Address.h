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

// Defines constants and types associated with addresses.

#ifndef DECOMPRESSOR_SRC_STREAM_ADDRESS_H_
#define DECOMPRESSOR_SRC_STREAM_ADDRESS_H_

#include "utils/Defs.h"

namespace wasm {

namespace decode {

typedef size_t AddressType;

static constexpr AddressType PageSizeLog2 =
#ifdef WASM_DECODE_PAGE_SIZE
    WASM_DECODE_PAGE_SIZE
#else
    16
#endif
    ;
static constexpr AddressType PageSize = 1 << PageSizeLog2;
static constexpr AddressType PageMask = PageSize - 1;

// Page index associated with address in queue.
constexpr AddressType PageIndex(size_t Address) {
  return Address >> PageSizeLog2;
}

// Returns address within a Page that refers to address.
constexpr AddressType PageAddress(size_t Address) {
  return Address & PageMask;
}

// Returns the minimum address for a page index.
constexpr AddressType minAddressForPage(AddressType PageIndex) {
  return PageIndex << PageSizeLog2;
}

// Note: We reserve the last page to be an "error" page. This allows us to
// guarantee that read/write cursors are always associated with a (defined)
// page.
static constexpr AddressType kMaxEofAddress = ~size_t(0) << PageSizeLog2;
static constexpr AddressType kMaxPageIndex = PageIndex(kMaxEofAddress);
static constexpr AddressType kErrorPageAddress = kMaxEofAddress + 1;
static constexpr AddressType kErrorPageIndex = PageIndex(kErrorPageAddress);
static constexpr AddressType kUndefinedAddress =
    std::numeric_limits<size_t>::max();

inline bool isGoodAddress(AddressType Addr) {
  return Addr <= kMaxEofAddress;
}
inline bool isDefinedAddress(AddressType Addr) {
  return Addr != kUndefinedAddress;
}
inline void resetAddress(AddressType& Addr) {
  Addr = 0;
}

void describeAddress(FILE* File, AddressType Addr);

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_ADDRESS_H_
