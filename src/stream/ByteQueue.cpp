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

#include "stream/ByteQueue.h"
#include "stream/QueueImpl.h"
#include "stream/WriteUtils.h"

namespace wasm {

namespace decode {

template class Queue<uint8_t>;

void ByteQueue::writePageAt(FILE* File, size_t Address) {
  std::shared_ptr<Page> P = getPage(Address);
  if (!P)
    return;
  size_t Size = Page::address(Address);
  size_t Count = 0;
  for (size_t i = 0; i < Size; ++i, ++Count) {
    if (Count % 16 == 0) {
      if (Count)
        fputc('\n', File);
    } else {
      fputc(' ', File);
    }
    writeInt(File, P->Buffer[i], ValueFormat::Hexidecimal);
  }
  fputc('\n', File);
}

size_t ByteQueue::read(size_t& Address, uint8_t* ToBuf, size_t WantedSize) {
  size_t Count = 0;
  PageCursor Cursor;
  while (WantedSize) {
    size_t FoundSize = readFromPage(Address, WantedSize, Cursor);
    if (FoundSize == 0)
      return Count;
    uint8_t* FromBuf = Cursor.getBufferPtr();
    memcpy(ToBuf, FromBuf, FoundSize);
    Count += FoundSize;
    WantedSize -= FoundSize;
    Address += FoundSize;
  }
  return Count;
}

bool ByteQueue::write(size_t& Address, uint8_t* FromBuf, size_t WantedSize) {
  PageCursor Cursor;
  while (WantedSize) {
    size_t FoundSize = writeToPage(Address, WantedSize, Cursor);
    if (FoundSize == 0)
      return false;
    uint8_t* ToBuf = Cursor.getBufferPtr();
    memcpy(ToBuf, FromBuf, FoundSize);
    Address += FoundSize;
    WantedSize -= FoundSize;
  }
  return true;
}

bool ReadBackedByteQueue::readFill(size_t Address) {
  if (Address < LastPage->getMaxAddress())
    return true;
  if (EobFrozen)
    return false;
  // Read fill if possible, until at least one byte is available.
  while (Address >= LastPage->getMaxAddress()) {
    if (size_t SpaceAvailable = LastPage->spaceRemaining()) {
      size_t NumBytes = Reader->read(
          &(LastPage->Buffer[Page::address(Address)]), SpaceAvailable);
      LastPage->incrementMaxAddress(NumBytes);
      if (NumBytes == 0) {
        EobFrozen = true;
        return false;
      }
      continue;
    }
    std::shared_ptr<Page> NewPage =
        std::make_shared<Page>(LastPage->getMaxAddress());
    std::weak_ptr<Page> PlaceHolder(NewPage);
    PageMap.push_back(PlaceHolder);
    LastPage->Next = NewPage;
    LastPage = NewPage;
  }
  return true;
}

WriteBackedByteQueue::~WriteBackedByteQueue() {
  // NOTE: we must override the base destructor so that calls to dumpFirstPage
  // is the one local to this class!
  while (FirstPage)
    dumpFirstPage();
}

void WriteBackedByteQueue::dumpFirstPage() {
  size_t Address = 0;
  size_t Size = FirstPage->getMaxAddress() - FirstPage->getMinAddress();
  if (!Writer->write(&FirstPage->Buffer[Address], Size))
    fatal("Write to raw stream failed in ByteQueue::dumpFirstPage");
  ByteQueue::dumpFirstPage();
}

}  // end of decode namespace

}  // end of wasm namespace
