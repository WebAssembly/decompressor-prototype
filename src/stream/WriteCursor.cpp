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

#include "stream/WriteCursor.h"

namespace wasm {

using namespace utils;

namespace decode {

WriteCursor::WriteCursor() : WriteCursorBase() {
}

WriteCursor::WriteCursor(std::shared_ptr<Queue> Que)
    : WriteCursorBase(StreamType::Byte, Que) {
}

WriteCursor::WriteCursor(StreamType Type, std::shared_ptr<Queue> Que)
    : WriteCursorBase(Type, Que) {
}

WriteCursor::WriteCursor(const WriteCursor& C) : WriteCursorBase(C) {
}

WriteCursor::WriteCursor(const Cursor& C, size_t StartAddress)
    : WriteCursorBase(C, StartAddress) {
}

WriteCursor::WriteCursor(const WriteCursorBase& C) : WriteCursorBase(C) {
}

WriteCursor::~WriteCursor() {
}

void WriteCursor::writeFillWriteByte(uint8_t Byte) {
  if (isIndexAtEndOfPage())
    writeFillBuffer();
  updateGuaranteedBeforeEob();
  writeOneByte(Byte);
}

}  // end of namespace decode

}  // end of namespace wasm
