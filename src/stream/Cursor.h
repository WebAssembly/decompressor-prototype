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

// Defines a pointer into a byte stream.

#ifndef DECOMPRESSOR_SRC_STREAM_CURSOR_H
#define DECOMPRESSOR_SRC_STREAM_CURSOR_H

#include "stream/ByteQueue.h"

namespace wasm {

namespace decode {

#define CURSOR_OLD 0

class Cursor {
  Cursor() = delete;
  Cursor& operator=(const Cursor&) = delete;

 public:
  Cursor(StreamType Type, ByteQueue* Queue)
      : Type(Type), Queue(Queue), EobPtr(Queue->getEofPtr()) {}
  explicit Cursor(const Cursor& C)
      : Type(C.Type), Queue(C.Queue), PgCursor(C.PgCursor), EobPtr(C.EobPtr) {}
  ~Cursor() {}

  void swap(Cursor& C) {
    std::swap(Type, C.Type);
    std::swap(Queue, C.Queue);
    std::swap(PgCursor, C.PgCursor);
    std::swap(EobPtr, C.EobPtr);
  }

  StreamType getType() const { return Type; }

  size_t getCurByteAddress() const { return PgCursor.getCurAddress(); }

  void reset() {}

  ByteQueue* getQueue() { return Queue; }

  void jumpToByteAddress(size_t NewAddress);

  bool atEob() {
    return PgCursor.getCurAddress() >= getEobAddress() || !readFillBuffer();
  }

  size_t getEobAddress() const { return EobPtr->getEobAddress(); }

  void setEobAddress(size_t NewValue) { EobPtr->setEobAddress(NewValue); }

  void pushEobAddress(size_t NewLocalEob) {
    EobPtr = std::make_shared<BlockEob>(NewLocalEob, EobPtr);
  }

  void popEobAddress() {
    EobPtr = EobPtr->getEnclosingEobPtr();
    assert(EobPtr);
  }

  // Reads next byte. Returns zero if at end of buffer.
  uint8_t readByte() {
    if (PgCursor.isIndexAtEndOfPage() && !readFillBuffer())
      return 0;
    return PgCursor.readByte();
  }

  // Writes next byte. Fails if at end of buffer.
  void writeByte(uint8_t Byte) {
    if (PgCursor.isIndexAtEndOfPage())
      writeFillBuffer();
    PgCursor.writeByte(Byte);
  }

  void freezeEof() { Queue->freezeEof(PgCursor.getCurAddress()); }

  // For debugging.
  void writeCurPage(FILE* File);

  // For debugging.
  void describe() {
    size_t EobAddress = getEobAddress();
    fprintf(stderr, "Cursor ");
    if (EobAddress != kUndefinedAddress)
      fprintf(stderr, " eob=%" PRIuMAX " ", uintmax_t(EobAddress));
    PgCursor.describe();
  }

 protected:
  StreamType Type;
  // The byte queue the cursor points to.
  ByteQueue* Queue;
  // The current address into the buffer.
  PageCursor PgCursor;
  // End of block address.
  std::shared_ptr<BlockEob> EobPtr;

  // Returns true if able to fill the buffer with at least one byte.
  bool readFillBuffer();

  // Creates new pages in buffer so that writes can occur.
  void writeFillBuffer();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_CURSOR_H
