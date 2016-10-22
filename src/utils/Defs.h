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
#include <cinttypes>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace wasm {

#ifdef NDEBUG
inline bool isRelease() {
  return true;
}
inline bool isDebug() {
  return false;
}
#else
inline bool isRelease() {
  return false;
}
inline bool isDebug() {
  return true;
}
#endif

template <class T, size_t N>
size_t size(T(&)[N]) {
  return N;
}

template <typename T>
constexpr T const_maximum(T V) {
  return V;
}

template <typename T>
constexpr T const_max(T V1, T V2) {
  return V1 < V2 ? V2 : V1;
}

template <typename T, typename... Args>
constexpr T const_maximum(T V, Args... args) {
  return const_max(V, const_maximum(args...));
}

static constexpr uint32_t WasmBinaryMagic = 0x6d736100;
static constexpr uint32_t WasmBinaryVersion = 0x0b;

static constexpr uint32_t CasmBinaryMagic = 0x6d736163;
static constexpr uint32_t CasmBinaryVersion = 0x0;

enum class Ordering : int {
  LessThan = -1,
  Equal = 0,
  GreaterThan = 1,
  NotComparable = 2
};

namespace decode {

typedef uint64_t IntType;
typedef int64_t SignedIntType;

#define PRI_IntType PRIu64
static constexpr size_t kBitsInIntType = 64;

enum class StreamType : uint8_t { Byte, Int };
const char* getName(StreamType Type);
Ordering compare(StreamType S1, StreamType S2);

enum class StreamKind : uint8_t { Input, Output };
const char* getName(StreamKind Type);

// When true, converts EXIT_FAIL to EXIT_SUCCESS, and EXIT_SUCCESS to EXIT_FAIL.
extern bool ExpectExitFail;

// Converts exit status, based on ExpectExitFail.
int exit_status(int Status);

void fatal(const char* Message = "fatal: unable to continue");

void fatal(const std::string& Message);

struct Utils {
  static constexpr size_t floorByte(size_t Bit) { return Bit >> 3; }

  static size_t ceilByte(size_t Bit) {
    size_t Byte = floorByte(Bit);
    return (Byte & 0x7) ? Byte + 1 : Byte;
  }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_DEFS_H
