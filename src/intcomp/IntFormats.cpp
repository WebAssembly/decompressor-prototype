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
#include "interp/FormatHelpers.h"

namespace wasm {

using namespace decode;
using namespace interp;

namespace intcomp {

namespace {

const char* IntTypeFormatName[NumIntTypeFormats] = {
    "uint8",
    "varint32",
    "varuint32",
    "uint32",
    "varint64",
    "varuint64"
    "uint64",
};

template <class T>
bool isInstanceOf(IntType Value) {
  return Value == IntType(T(Value));
}

class TestBuffer {
  TestBuffer(const TestBuffer&) = delete;
  TestBuffer& operator-(const TestBuffer&) = delete;

 public:
  TestBuffer() : Index(0) {}
  void reset() { Index = 0; }
  void writeByte(uint8_t Byte) {
    (void)Byte;
    ++Index;
  }
  size_t getSize() const { return Index; }
  size_t writeVarint32(uint32_t Value) {
    reset();
    fmt::writeVarint32(Value, *this);
    return getSize();
  }
  size_t writeVaruint32(uint32_t Value) {
    reset();
    fmt::writeVaruint32(Value, *this);
    return getSize();
  }
  size_t writeVarint64(uint64_t Value) {
    reset();
    fmt::writeVarint64(Value, *this);
    return getSize();
  }
  size_t writeVaruint64(uint64_t Value) {
    reset();
    fmt::writeVaruint64(Value, *this);
    return getSize();
  }

 private:
  size_t Index;
};

}  // end of anonymous namespace

const char* getName(IntTypeFormat Fmt) {
  assert(int(Fmt) <= int(IntTypeFormat::LAST));
  return IntTypeFormatName[size_t(Fmt)];
}

IntTypeFormats::IntTypeFormats(IntType Value) : Value(Value) {
  installValidByteSizes(Value);
}

void IntTypeFormats::installValidByteSizes(IntType Value) {
  // Initialize to unknownSize
  for (size_t i = 0; i < NumIntTypeFormats; ++i)
    ByteSize[i] = UnknownSize;
  TestBuffer Buffer;
  if (isInstanceOf<uint8_t>(Value))
    ByteSize[size_t(IntTypeFormat::Uint8)] = sizeof(uint8_t);
  if (isInstanceOf<uint32_t>(Value)) {
    ByteSize[size_t(IntTypeFormat::Uint32)] = sizeof(uint32_t);
    ByteSize[size_t(IntTypeFormat::Varint32)] =
        Buffer.writeVarint32(uint32_t(Value));
    ByteSize[size_t(IntTypeFormat::Varuint32)] =
        Buffer.writeVaruint32(uint32_t(Value));
  }
  if (isInstanceOf<uint64_t>(Value)) {
    ByteSize[size_t(IntTypeFormat::Uint64)] = sizeof(uint64_t);
    ByteSize[size_t(IntTypeFormat::Varint64)] =
        Buffer.writeVarint64(uint64_t(Value));
    ByteSize[size_t(IntTypeFormat::Varuint64)] =
        Buffer.writeVaruint64(uint64_t(Value));
  }
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
