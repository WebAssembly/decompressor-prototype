/* -*- C++ -*- */
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

// Defines a pipe that associates a write queue with a read queue.

// The pipe is set up to not copy output on the write queue to the
// read queue, until all write cursors can no longer reference the
// contents.

#ifndef DECOMPRESSOR_SRC_STREAM_PIPE_H
#define DECOMPRESSOR_SRC_STREAM_PIPE_H

#include "stream/Queue.h"
#include "stream/WriteCursor2ReadQueue.h"

namespace wasm {

namespace decode {

class Pipe FINAL {
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;

 public:
  Pipe()
      : Input(std::make_shared<Queue>()),
        Output(std::make_shared<PipeBackedQueue>(*this)),
        WritePos(Output) {}
  ~Pipe() {}
  std::shared_ptr<Queue> getInput() const { return Input; }
  std::shared_ptr<Queue> getOutput() const { return Output; }
  class PipeBackedQueue FINAL : public Queue {
    PipeBackedQueue(const PipeBackedQueue&) = delete;
    PipeBackedQueue& operator=(const PipeBackedQueue&) = delete;
    PipeBackedQueue() = delete;

   public:
    PipeBackedQueue(Pipe& MyPipe) : Queue(), MyPipe(MyPipe) {}
    ~PipeBackedQueue();

   private:
    Pipe& MyPipe;
    void dumpFirstPage() OVERRIDE;
  };

 private:
  std::shared_ptr<Queue> Input;
  std::shared_ptr<PipeBackedQueue> Output;
  WriteCursor2ReadQueue WritePos;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_PIPE_H
