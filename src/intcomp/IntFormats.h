/* -*- C++ -*- */
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

// Defines possible formats (and the number of bytes needed) for each
// integer value.

#ifndef DECOMPRESSOR_SRC_INTCOMP_INTFORMATS_H
#define DECOMPRESSOR_SRC_INTCOMP_INTFORMATS_H

#include "utils/Defs.h"

namespace wasm {

namespace intcomp {

enum class IntTypeFormat {
  // Note: Sizes are listed in order of preference, if they contain the
  // same number of bytes.
  Uint8,
  Uint32,
  Uint64,
  Varuint32,
  Varint32,
  Varuint64,
  Varint64,
  LAST = Varint64  // Note: worst case choice.
};

const char* getName(IntTypeFormat Fmt);

static constexpr size_t NumIntTypeFormats = size_t(IntTypeFormat::LAST) + 1;

class IntTypeFormats {
  IntTypeFormats() = delete;
  IntTypeFormats(const IntTypeFormats&) = delete;
  IntTypeFormats& operator=(const IntTypeFormats&) = delete;

 public:
  static constexpr int UnknownSize = 0;
  IntTypeFormats(decode::IntType Value);

  decode::IntType getValue() const { return Value; }
  int getByteSize(IntTypeFormat Fmt) const {
    return ByteSize[size_t(Fmt)];
  }

  // Returns the preferred format, based on the number of bytes.
  IntTypeFormat getFirstMinimumFormat() const;

  // Returns next format with same size, or Fmt if no more candidates.
  IntTypeFormat getNextMatchingFormat(IntTypeFormat Fmt) const;

 private:
  decode::IntType Value;
  int ByteSize[NumIntTypeFormats];

  void installValidByteSizes(decode::IntType Value);
};

}  // end of namespace intcomp

}  // end of namespace wasm


#endif // DECOMPRESSOR_SRC_INTCOMP_INTFORMATS_H
