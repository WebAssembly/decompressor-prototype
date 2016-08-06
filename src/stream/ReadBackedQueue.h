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
//

#ifndef DECOMPRESSOR_SRC_STREAM_READBACKEDQUEUE_H
#define DECOMPRESSOR_SRC_STREAM_READBACKEDQUEUE_H

#include "stream/Queue.h"
#include "stream/RawStream.h"

namespace wasm {

namespace decode {

// Read-only queue that is write-filled from a steam using the given
// Reader.
class ReadBackedQueue FINAL : public Queue {
  ReadBackedQueue(const ReadBackedQueue&) = delete;
  ReadBackedQueue& operator=(const ReadBackedQueue&) = delete;
  ReadBackedQueue() = delete;

 public:
  ReadBackedQueue(std::unique_ptr<RawStream> _Reader) {
    assert(_Reader);
    Reader = std::move(_Reader);
  }
  ~ReadBackedQueue() OVERRIDE {}

 private:
  // Reader to write fill buffer as needed.
  std::unique_ptr<RawStream> Reader;

  bool readFill(size_t Address) OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_READBACKEDQUEUE_H
