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

// Defines a pointer to a byte stream for writing, where the byte stream
// is the input to a later step. Hence, it doesn't advance the byte stream
// beyond defined results, so that the corresponding read cursors can
// assume all contents is defined.

#ifndef DECOMPRESSOR_SRC_STREAM_WRITECURSOR2READQUEUE_H_
#define DECOMPRESSOR_SRC_STREAM_WRITECURSOR2READQUEUE_H_

#include "stream/WriteCursorBase.h"

namespace wasm {

namespace decode {

class WriteCursor2ReadQueue FINAL : public WriteCursorBase {
 public:
  // Note: The nullary write cursor should not be used until it has been
  // assigned a value.
  WriteCursor2ReadQueue();
  WriteCursor2ReadQueue(std::shared_ptr<Queue> Que);
  WriteCursor2ReadQueue(StreamType Type, std::shared_ptr<Queue> Que);
  explicit WriteCursor2ReadQueue(const WriteCursor2ReadQueue& C);
  WriteCursor2ReadQueue(const Cursor& C, AddressType StartAddress);
  ~WriteCursor2ReadQueue() OVERRIDE;

  WriteCursor2ReadQueue& operator=(const WriteCursor2ReadQueue& C) {
    WriteCursorBase::assign(C);
    return *this;
  }

 protected:
  void writeFillWriteByte(ByteType Byte) OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_WRITECURSOR2READQUEUE_H_
