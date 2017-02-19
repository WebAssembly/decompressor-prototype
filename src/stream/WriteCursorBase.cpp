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

#include "stream/WriteCursorBase.h"

namespace wasm {

namespace decode {

WriteCursorBase::WriteCursorBase() : Cursor() {
}

WriteCursorBase::WriteCursorBase(std::shared_ptr<Queue> Que)
    : Cursor(StreamType::Byte, Que) {
}

WriteCursorBase::WriteCursorBase(StreamType Type, std::shared_ptr<Queue> Que)
    : Cursor(Type, Que) {
}

WriteCursorBase::WriteCursorBase(const WriteCursorBase& C) : Cursor(C) {
}

WriteCursorBase::WriteCursorBase(const Cursor& C, size_t StartAddress)
    : Cursor(C, StartAddress, false) {
}

WriteCursorBase::~WriteCursorBase() {
}

void WriteCursorBase::writeByte(uint8_t Byte) {
  if (CurAddress < GuaranteedBeforeEob)
    writeOneByte(Byte);
  else
    writeFillWriteByte(Byte);
}

void WriteCursorBase::writeOneByte(uint8_t Byte) {
  *getBufferPtr() = Byte;
  ++CurAddress;
}

}  // end of namespace decode

}  // end of namespace wasm
