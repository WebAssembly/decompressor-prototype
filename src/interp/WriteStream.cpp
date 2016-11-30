// -*- C++ -*- */
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

// Implements defaults for stream writers.

#include "interp/WriteStream.h"

#include "interp/FormatHelpers.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

WriteStream::~WriteStream() {
}

void WriteStream::writeUint8(uint8_t Value, WriteCursor& Pos) {
  Pos.writeByte(uint8_t(Value));
}

void WriteStream::writeUint32(uint32_t Value, WriteCursor& Pos) {
  writeFixed<uint32_t>(Value, Pos);
}

void WriteStream::writeUint64(uint64_t Value, WriteCursor& Pos) {
  writeFixed<uint64_t>(Value, Pos);
}

void WriteStream::writeVarint32(int32_t Value, WriteCursor& Pos) {
  if (Value < 0)
    writeNegativeLEB128<int32_t>(Value, Pos);
  else
    writePositiveLEB128<int32_t>(Value, Pos);
}

void WriteStream::writeVarint64(int64_t Value, WriteCursor& Pos) {
  if (Value < 0)
    writeNegativeLEB128<int64_t>(Value, Pos);
  else
    writePositiveLEB128<int64_t>(Value, Pos);
}

void WriteStream::writeVaruint32(uint32_t Value, WriteCursor& Pos) {
  writeLEB128<uint32_t>(Value, Pos);
}

void WriteStream::writeVaruint64(uint64_t Value, WriteCursor& Pos) {
  writeLEB128<uint64_t>(Value, Pos);
}

void WriteStream::writeFixedVaruint32(uint32_t Value, WriteCursor& Pos) {
  writeFixedLEB128<uint32_t>(Value, Pos);
}

}  // end of namespace decode

}  // end of namespace wasm
