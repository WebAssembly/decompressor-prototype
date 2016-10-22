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

// Implements a byte stream reader.

#include "interp/ByteReadStream.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

ByteReadStream::~ByteReadStream() {
}

IntType ByteReadStream::readValue(ReadCursor& Pos,
                                  const filt::Node* Format) {
  switch (Format->getType()) {
    case OpUint32:
      return readUint32(Pos);
    case OpUint64:
      return readUint64(Pos);
    case OpUint8:
      return readUint8(Pos);
    case OpVarint32:
      return readVarint32(Pos);
    case OpVarint64:
      return readVarint64(Pos);
    case OpVaruint32:
      return readVaruint32(Pos);
    case OpVaruint64:
      return readVaruint64(Pos);
    default:
      fatal("readValue not defined for format!");
      return 0;
  }
}

size_t ByteReadStream::readBlockSize(ReadCursor& Pos) {
  ReadCursor Peek(Pos);
  fprintf(stderr, "read block size @%" PRIxMAX ":", uintmax_t(Peek.getCurAddress()));
  for (int i = 0; i < 5; ++i) {
    uint8_t Byte = Peek.readByte();
    fprintf(stderr, " %u = 0x%x\n", Byte, Byte);
  }
  fprintf(stderr, "\n");
  return readVaruint32(Pos);
}

void ByteReadStream::pushEobAddress(decode::ReadCursor& Pos, size_t Address) {
  Pos.pushEobAddress(Pos.getCurByteAddress() + Address);
}

}  // end of namespace decode

}  // end of namespace wasm
