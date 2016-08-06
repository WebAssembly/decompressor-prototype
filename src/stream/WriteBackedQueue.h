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

#ifndef DECOMPRESSOR_SRC_STREAM_WRITEBACKEDQUEUE_H
#define DECOMPRESSOR_SRC_STREAM_WRITEBACKEDQUEUE_H

#include "stream/Queue.h"
#include "stream/RawStream.h"

namespace wasm {

namespace decode {

// Write-only queue that is dumped to a stream using the given Writer.
class WriteBackedQueue FINAL : public Queue {
  WriteBackedQueue(const WriteBackedQueue&) = delete;
  WriteBackedQueue& operator=(const WriteBackedQueue&) = delete;
  WriteBackedQueue() = delete;

 public:
  WriteBackedQueue(std::unique_ptr<RawStream> _Writer) {
    assert(_Writer);
    Writer = std::move(_Writer);
  }
  ~WriteBackedQueue();

 private:
  // Writer to dump contents of queue, when the contents is no longer
  // needed by reader.
  std::unique_ptr<RawStream> Writer;

  void dumpFirstPage() OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_WRITEBACKEDQUEUE_H
