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

// Implements possible formats (and the number of bytes needed) for
// Implementseach integer value.

#include "intcomp/IntFormats.h"

namespace wasm {

using namespace decode;

namespace intcomp {

namespace {

const char* IntTypeFormatName[NumIntTypeFormats] = {
  "uint8",
  "uint32",
  "uint64",
  "varint32",
  "varint64",
  "varuint32",
  "varuint64"
};

template<class T> bool isInstanceOf(IntType Value) {
  return Value == IntType(T(Value));
}

} // end of anonymous namespace

const char* getName(IntTypeFormat Fmt) {
  assert(int(Fmt) <= int(IntTypeFormat::LAST));
  return IntTypeFormatName[size_t(Fmt)];
}

IntTypeFormats::IntTypeFormats(IntType Value)
    : Value(Value) {
  // Initialize to unknownSize
  for (size_t i = 0; i < NumIntTypeFormats; ++i)
    ByteSize[i] = UnknownSize;
  installValidByteSizes(Value);
}

void IntTypeFormats::installValidByteSizes(IntType Value) {
  if (isInstanceOf<size_t>(Value))
    ByteSize[size_t(IntTypeFormat::Uint8)] = sizeof(uint8_t);
  if (isInstanceOf<size_t>(Value))
    ByteSize[size_t(IntTypeFormat::Uint32)] = sizeof(uint32_t);
}

IntTypeFormat IntTypeFormats::getFirstMinimumFormat() const {
  IntTypeFormat Fmt = IntTypeFormat::LAST;
  // Initialize minimum byte size with number guaranteed to be larger
  // than any known byte count for a format.
  int MinByteSize = sizeof(uint64_t) * 2;
  for (size_t i = 0; i < NumIntTypeFormats; ++i) {
    if (ByteSize[i] != UnknownSize && ByteSize[i] < MinByteSize) {
      Fmt = IntTypeFormat(i);
      MinByteSize = ByteSize[i];
    }
  }
  return Fmt;
}

IntTypeFormat IntTypeFormats::getNextMatchingFormat(IntTypeFormat Fmt) const {
  assert(int(Fmt) <= int(IntTypeFormat::LAST));
  int WantedSize = ByteSize[size_t(Fmt)];
  for (size_t i = size_t(Fmt) + 1; i < NumIntTypeFormats; ++i) {
    if (WantedSize == ByteSize[i])
      return IntTypeFormat(i);
  }
  return Fmt;
}

}  // end of namespace intcomp

}  // end of namespace wasm
