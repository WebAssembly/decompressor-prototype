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

#include "stream/Pipe.h"

#include "stream/Page.h"
#include "stream/Queue.h"
#include "stream/WriteCursor2ReadQueue.h"

namespace wasm {

namespace decode {

class Pipe::PipeBackedQueue FINAL : public Queue {
  PipeBackedQueue(const PipeBackedQueue&) = delete;
  PipeBackedQueue& operator=(const PipeBackedQueue&) = delete;
  PipeBackedQueue() = delete;

 public:
  PipeBackedQueue(Pipe& MyPipe);
  ~PipeBackedQueue();

 private:
  Pipe& MyPipe;
  void dumpFirstPage() OVERRIDE;
};

Pipe::PipeBackedQueue::PipeBackedQueue(Pipe& MyPipe)
    : Queue(), MyPipe(MyPipe) {}

void Pipe::PipeBackedQueue::dumpFirstPage() {
  // TODO(karlschimpf) Optimize this!
  for (AddressType i = 0, Size = FirstPage->getPageSize(); i < Size; ++i)
    MyPipe.WritePos->writeByte(FirstPage->getByte(i));
  Queue::dumpFirstPage();
}

Pipe::Pipe()
    : Input(std::make_shared<PipeBackedQueue>(*this)),
      Output(std::make_shared<Queue>()),
      WritePos(utils::make_unique<WriteCursor2ReadQueue>(Output)) {}

Pipe::~Pipe() {}

std::shared_ptr<Queue> Pipe::getInput() const {
  return Input;
}

std::shared_ptr<Queue> Pipe::getOutput() const {
  return Output;
}

Pipe::PipeBackedQueue::~PipeBackedQueue() {
  // NOTE: we must override the base destructor so that calls to dumpFirstPage
  // is the one local to this class!
  close();
}

}  // end of decode namespace

}  // end of wasm namespace
