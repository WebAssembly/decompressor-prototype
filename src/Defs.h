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

/* Defines common definitions used by the decompressor. */

#ifndef DECOMPRESSOR_SRC_DEFS_H
#define DECOMPRESSOR_SRC_DEFS_H

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>

template<class T, size_t N>
size_t size(T (&)[N]) { return N; }

namespace wasm {

namespace decode {

using IntType = uint64_t;
static constexpr size_t kBitsInIntType = 64;

enum class StreamType {Bit, Byte, Int, Ast};

void fatal(const char *Message = "fatal: unable to continue");

struct Utils {
  static constexpr size_t floorByte(size_t Bit) {
    return Bit >> 3;
  }

  static size_t ceilByte(size_t Bit) {
    size_t Byte = floorByte(Bit);
    return (Byte & 0x7)
        ? Byte + 1
        : Byte;
  }
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_DEFS_H
