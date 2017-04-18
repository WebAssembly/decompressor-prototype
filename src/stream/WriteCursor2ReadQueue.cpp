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

#include "stream/WriteCursor2ReadQueue.h"

namespace wasm {

namespace decode {

WriteCursor2ReadQueue::WriteCursor2ReadQueue() : WriteCursorBase() {}

WriteCursor2ReadQueue::WriteCursor2ReadQueue(std::shared_ptr<Queue> Que)
    : WriteCursorBase(StreamType::Byte, Que) {}

WriteCursor2ReadQueue::WriteCursor2ReadQueue(StreamType Type,
                                             std::shared_ptr<Queue> Que)
    : WriteCursorBase(Type, Que) {}

WriteCursor2ReadQueue::WriteCursor2ReadQueue(const WriteCursor2ReadQueue& C)
    : WriteCursorBase(C) {}

WriteCursor2ReadQueue::WriteCursor2ReadQueue(const Cursor& C,
                                             AddressType StartAddress)
    : WriteCursorBase(C, StartAddress) {}

WriteCursor2ReadQueue::~WriteCursor2ReadQueue() {}

void WriteCursor2ReadQueue::writeFillWriteByte(ByteType Byte) {
  if (isIndexAtEndOfPage())
    writeFillBuffer(1);
  updateGuaranteedBeforeEob();
  writeOneByte(Byte);
}

}  // end of namespace decode

}  // end of namespace wasm
