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

// TODO(karlschimpf): Consider building a cache for numbers, to
// cut down the number of calls to get the minimum size.

const char* IntTypeFormatName[NumIntTypeFormats] = {
    "uint8",
    "varint32",
    "varuint32",
    "uint32",
    "varint64",
    "varuint64",
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
  size_t Index = size_t(Fmt);
  assert(Index < NumIntTypeFormats);
  return IntTypeFormatName[Index];
}

IntTypeFormats::IntTypeFormats(IntType Value) : Value(Value) {
  installValidByteSizes(Value);
}

size_t IntTypeFormats::getByteSize(IntTypeFormat Fmt) const {
  size_t Index = size_t(Fmt);
  if (!Cached[Index])
    cacheFormat(Fmt);
  return ByteSize[Index];
}

void IntTypeFormats::cacheFormat(IntTypeFormat Fmt) const {
  size_t Index = size_t(Fmt);
  Cached[Index] = true;
  TestBuffer Buffer;
  switch (Fmt) {
    case IntTypeFormat::Uint8:
      ByteSize[Index] =
          isInstanceOf<uint8_t>(Value) ? sizeof(uint8_t) : NotValid;
      break;
    case IntTypeFormat::Uint32:
      ByteSize[Index] =
          isInstanceOf<uint32_t>(Value) ? sizeof(uint32_t) : NotValid;
      break;
    case IntTypeFormat::Uint64:
      ByteSize[Index] =
          isInstanceOf<uint64_t>(Value) ? sizeof(uint64_t) : NotValid;
      break;
    case IntTypeFormat::Varint32:
      ByteSize[Index] = isInstanceOf<int32_t>(Value)
                            ? Buffer.writeVarint32(int32_t(Value))
                            : NotValid;
      break;
    case IntTypeFormat::Varuint32:
      ByteSize[Index] = isInstanceOf<uint32_t>(Value)
                            ? Buffer.writeVaruint32(uint32_t(Value))
                            : NotValid;
      break;
    case IntTypeFormat::Varint64:
      ByteSize[Index] = isInstanceOf<int64_t>(Value)
                            ? Buffer.writeVarint64(int64_t(Value))
                            : NotValid;
      break;
    case IntTypeFormat::Varuint64:
      ByteSize[Index] = isInstanceOf<uint64_t>(Value)
                            ? Buffer.writeVaruint64(uint64_t(Value))
                            : NotValid;
      break;
  }
}

void IntTypeFormats::installValidByteSizes(IntType Value) {
  // Initialize to unknownSize
  for (size_t i = 0; i < NumIntTypeFormats; ++i) {
    ByteSize[i] = NotValid;
    Cached[i] = false;
  }
}

IntTypeFormat IntTypeFormats::getFirstMinimumFormat() const {
  IntTypeFormat MinFmt = IntTypeFormat::LAST;
  size_t MinSize = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i < NumIntTypeFormats; ++i) {
    IntTypeFormat Fmt = IntTypeFormat(i);
    size_t FmtSize = getByteSize(Fmt);
    if (FmtSize == NotValid)
      continue;
    if (FmtSize < MinSize) {
      MinFmt = Fmt;
      MinSize = FmtSize;
    }
  }
  return MinFmt;
}

IntTypeFormat IntTypeFormats::getNextMatchingFormat(IntTypeFormat Fmt) const {
  assert(size_t(Fmt) <= size_t(IntTypeFormat::LAST));
  size_t FmtSize = getByteSize(Fmt);
  for (size_t i = size_t(Fmt) + 1; i < NumIntTypeFormats; ++i) {
    IntTypeFormat NextFmt = IntTypeFormat(i);
    size_t NextSize = getByteSize(NextFmt);
    if (NextSize == FmtSize)
      return NextFmt;
  }
  return Fmt;
}

}  // end of namespace intcomp

}  // end of namespace wasm
