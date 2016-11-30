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
  Uint8,
  Uint32,
  Uint64,
  Varint32,
  Varint64,
  Varuint32,
  Varuint64,
  LAST = Varuint64
};

const char* getName(IntTypeFormat Fmt);

static constexpr size_t NumIntTypeFormats = size_t(IntTypeFormat::LAST) + 1;

class IntTypeFormats {
  IntTypeFormats() = delete;
  IntTypeFormats(const IntTypeFormats&) = delete;
  IntTypeFormats& operator=(const IntTypeFormats&) = delete;

 public:
  IntTypeFormats(decode::IntType Value);

 private:
  decode::IntType Value;
  int ByteSize[NumIntTypeFormats];
};

}  // end of namespace intcomp

}  // end of namespace wasm


#endif // DECOMPRESSOR_SRC_INTCOMP_INTFORMATS_H
