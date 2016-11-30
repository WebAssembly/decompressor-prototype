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

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

ReadStream::~ReadStream() {
}

uint8_t ReadStream::readUint8(ReadCursor& Pos) {
  return Pos.readByte();
}

uint32_t ReadStream::readUint32(ReadCursor& Pos) {
  return readFixed<uint32_t>(Pos);
}

uint64_t ReadStream::readUint64(ReadCursor& Pos) {
  return readFixed<uint64_t>(Pos);
}

int32_t ReadStream::readVarint32(ReadCursor& Pos) {
  return readSignedLEB128<uint32_t>(Pos);
}

int64_t ReadStream::readVarint64(ReadCursor& Pos) {
  return readSignedLEB128<uint64_t>(Pos);
}

uint32_t ReadStream::readVaruint32(ReadCursor& Pos) {
  return readLEB128<uint32_t>(Pos);
}

uint64_t ReadStream::readVaruint64(ReadCursor& Pos) {
  return readLEB128<uint64_t>(Pos);
}

}  // end of namespace interp

}  // end of namespace wasm
