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
#include "sexp/Ast.h"
#include "stream/WriteCursor.h"
#include "utils/Casting.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

WriteStream::WriteStream(decode::StreamType Type) : Type(Type) {
}

WriteStream::~WriteStream() {
}

void WriteStream::writeBit(uint8_t Value, WriteCursor& Pos) {
  Pos.writeBit(Value);
}

void WriteStream::writeUint8(uint8_t Value, WriteCursor& Pos) {
  fmt::writeUint8(Value, Pos);
}

void WriteStream::writeUint32(uint32_t Value, WriteCursor& Pos) {
  fmt::writeUint32(Value, Pos);
}

void WriteStream::writeUint64(uint64_t Value, WriteCursor& Pos) {
  fmt::writeUint64(Value, Pos);
}

void WriteStream::writeVarint32(int32_t Value, WriteCursor& Pos) {
  fmt::writeVarint32(Value, Pos);
}

void WriteStream::writeVarint64(int64_t Value, WriteCursor& Pos) {
  fmt::writeVarint64(Value, Pos);
}

void WriteStream::writeVaruint32(uint32_t Value, WriteCursor& Pos) {
  fmt::writeVaruint32(Value, Pos);
}

void WriteStream::writeVaruint64(uint64_t Value, WriteCursor& Pos) {
  fmt::writeVaruint64(Value, Pos);
}

void WriteStream::writeFixedVaruint32(uint32_t Value, WriteCursor& Pos) {
  fmt::writeFixedVaruint32(Value, Pos);
}

}  // end of namespace decode

}  // end of namespace wasm
