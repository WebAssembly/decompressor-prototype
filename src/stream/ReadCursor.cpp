// -*- C++ -*-
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

#include "stream/ReadCursor.h"

namespace wasm {

using namespace utils;

namespace decode {

uint8_t ReadCursor::readByteAfterReadFill() {
  bool atEof = isIndexAtEndOfPage() && !readFillBuffer();
  updateGuaranteedBeforeEob();
  if (atEof)
    return 0;
  return readOneByte();
}

size_t ReadCursor::advance(size_t Distance) {
  size_t WantedAddress = CurAddress + Distance;
  size_t DistanceMoved = 0;
  while (CurAddress < WantedAddress && CurAddress < Que->getEofAddress()) {
    size_t Size = Que->readFromPage(CurAddress, Page::Size, *this);
    if (Size == 0)
      break;
    CurAddress += Size;
    DistanceMoved += Size;
    CurPage = Que->getReadPage(CurAddress);
  }
  return DistanceMoved;
}

uint8_t ReadCursor::readByte() {
  return (CurAddress < GuaranteedBeforeEob) ? readOneByte()
                                            : readByteAfterReadFill();
}

uint8_t ReadCursor::readOneByte() {
  assert(CurPage);
  uint8_t Byte = *getBufferPtr();
  ++CurAddress;
  return Byte;
}

}  // end of namespace decode

}  // end of namespace wasm
