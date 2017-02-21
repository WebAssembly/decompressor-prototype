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

// Implements defaults for stream readers.

#include "interp/ReadStream.h"

#include "interp/FormatHelpers.h"
#include "stream/ReadCursor.h"
#include "utils/Casting.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

ReadStream::~ReadStream() {
}

uint8_t ReadStream::readBit(ReadCursor& Pos) {
  return Pos.readBit();
}

uint8_t ReadStream::readUint8(ReadCursor& Pos) {
  return fmt::readUint8(Pos);
}

uint32_t ReadStream::readUint32(ReadCursor& Pos) {
  return fmt::readUint32(Pos);
}

uint64_t ReadStream::readUint64(ReadCursor& Pos) {
  return fmt::readUint64(Pos);
}

int32_t ReadStream::readVarint32(ReadCursor& Pos) {
  return fmt::readVarint32(Pos);
}

int64_t ReadStream::readVarint64(ReadCursor& Pos) {
  return fmt::readVarint64(Pos);
}

uint32_t ReadStream::readVaruint32(ReadCursor& Pos) {
  return fmt::readVaruint32(Pos);
}

uint64_t ReadStream::readVaruint64(ReadCursor& Pos) {
  return fmt::readVaruint64(Pos);
}

}  // end of namespace interp

}  // end of namespace wasm
