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

#include "stream/ReadBackedQueue.h"

namespace wasm {

namespace decode {

bool ReadBackedQueue::readFill(size_t Address) {
  // Double check that there isn't more to read.
  if (Address < LastPage->getMaxAddress())
    return true;
  if (EofFrozen)
    return false;
  while (Address >= LastPage->getMaxAddress()) {
    size_t SpaceAvailable = LastPage->spaceRemaining();
    // Create new page if current page full.
    if (SpaceAvailable == 0) {
      appendPage();
      SpaceAvailable = Page::Size;
    }
    size_t NumBytes = Reader->read(&(LastPage->Buffer[Page::address(Address)]),
                                   SpaceAvailable);
    LastPage->incrementMaxAddress(NumBytes);
    if (NumBytes == 0) {
      freezeEof(Address);
      return false;
    }
  }
  return true;
}

}  // end of decode namespace

}  // end of wasm namespace
