/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Implements a queue for hold buffers to streams.
#ifndef DECOMPRESSOR_SRC_STREAM_QUEUEIMPL_H
#define DECOMPRESSOR_SRC_STREAM_QUEUEIMPL_H

#include "stream/Queue.h"

namespace wasm {

namespace decode {

template <class Base>
Queue<Base>::Queue() {
  EobPage = FirstPage = new Page(0);
  PageMap.push_back(EobPage);
  // Double check that we can evenly fit elements of Base in a page.
  assert(Page::Size % sizeof(Base) == 0);
}

template <class Base>
Queue<Base>::~Queue() {
  while (FirstPage)
    dumpFirstPage();
}

template <class Base>
void Queue<Base>::dumpFirstPage() {
  Page* TmpPage = FirstPage;
  FirstPage = FirstPage->Next;
  delete TmpPage;
  if (FirstPage)
    FirstPage->Last = nullptr;
}

template <class Base>
void Queue<Base>::dumpPreviousPages(size_t Address) {
  Page* AddrPage = getPage(Address);
  while (FirstPage != AddrPage) {
    if (!FirstPage->isLocked())
      break;
    if (FirstPage->getMaxAddress() + MinPeekSize < Address)
      break;
    dumpFirstPage();
  }
}

template <class Base>
bool Queue<Base>::readFill(size_t Address) {
  return Address < EobPage->getMaxAddress();
}

template <class Base>
Base* Queue<Base>::getReadLockedPointer(size_t Address,
                                        size_t WantedSize,
                                        size_t& LockedSize) {
  // Start by read-filling if necessary.
  if (Address >= EobPage->getMaxAddress()) {
    if (!readFill(Address)) {
      LockedSize = 0;
      return nullptr;
    }
  }
  // Find page associated with Address.
  Page* ReadPage = getPage(Address);
  if (ReadPage == nullptr ||
      (!LockedPages.empty() && ReadPage->Index < LockedPages.top())) {
    LockedSize = 0;
    return nullptr;
  }
  lockPage(ReadPage);
  dumpPreviousPages(Address);
  // Compute largest contiguous range of elements available.
  WantedSize *= sizeof(Base);
  LockedSize = WantedSize;
  if (Address + WantedSize > ReadPage->getMaxAddress()) {
    LockedSize = (ReadPage->getMaxAddress() - Address) / sizeof(Base);
  }
  return (Base*)(ReadPage->Buffer + Page::address(Address));
}

template <class Base>
Base* Queue<Base>::getWriteLockedPointer(size_t Address,
                                         size_t WantedSize,
                                         size_t& LockedSize) {
  // Page doesn't exist. Expand queue if necessary.
  while (Address >= EobPage->getMaxAddress()) {
    if (EobFrozen) {
      LockedSize = 0;
      return nullptr;
    }
    EobPage->setMaxAddress(EobPage->getMinAddress() + Page::Size);
    if (Address < EobPage->getMaxAddress())
      break;
    Page* NewPage = new Page(EobPage->getMaxAddress());
    PageMap.push_back(NewPage);
    EobPage->Next = NewPage;
    NewPage->Last = EobPage;
    EobPage = NewPage;
  }
  Page* P = getPage(Address);
  if (P == nullptr) {
    LockedSize = 0;
    return nullptr;
  }
  lockPage(P);
  dumpPreviousPages(Address);
  // Compute largest contiguous range of bytes available.
  WantedSize *= sizeof(Base);
  LockedSize = WantedSize;
  if (Address + WantedSize > P->getMaxAddress()) {
    LockedSize = (P->getMaxAddress() - Address) / sizeof(Base);
  }
  return (Base*)(P->Buffer + Page::address(Address));
}

template <class Base>
void Queue<Base>::freezeEob(size_t Address) {
  assert(!EobFrozen);
  size_t LockedSize;
  // This call zero-fill pages if writing hasn't reached Address yet.
  assert(getWriteLockedPointer(Address, 0, LockedSize) != nullptr);
  Page* P = getPage(Address);
  P->setMaxAddress(Address);
  unlock(Address);
}

}  // end of decode namespace

}  // end of wasm namespace

#endif  // DECOMPRESSOR_SRC_STREAM_QUEUEIMPL_H
