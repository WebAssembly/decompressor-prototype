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

#include "stream/WriteBackedQueue.h"

namespace wasm {

namespace decode {

WriteBackedQueue::~WriteBackedQueue() {
  // NOTE: we must override the base destructor so that calls to dumpFirstPage
  // is the one local to this class!
  close();
}

void WriteBackedQueue::dumpFirstPage() {
  size_t Address = 0;
  size_t Size = FirstPage->getMaxAddress() - FirstPage->getMinAddress();
  if (!Writer->write(FirstPage->getByteAddress(Address), Size))
    fail();
  Queue::dumpFirstPage();
}

}  // end of decode namespace

}  // end of wasm namespace
