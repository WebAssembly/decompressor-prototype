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

// TODO(karlschimpf): Move the implementation back into class Cursor, so
// that we don't need to allocate an implementation for each instruction
// peek, made by the decompressor.
//
// Base class for the internal implementation of cursors.
class CursorImpl {
  CursorImpl(const CursorImpl&) = delete;
  CursorImpl& operator=(const CursorImpl&) = delete;
  CursorImpl() = delete;

 public:
  CursorImpl(StreamType Type, ByteQueue* Queue) :
      Type(Type), Queue(Queue), EobPtr(Queue->getEofPtr()) {}

  CursorImpl(StreamType Type, ByteQueue* Queue, const PageCursor& PgCursor)
      : Type(Type), Queue(Queue), PgCursor(PgCursor),
        EobPtr(Queue->getEofPtr()) {}

  virtual ~CursorImpl() {}

  // Note: Assumes that cursor is byte aligned when called.
  uint8_t readByte() {
    if (PgCursor.isIndexAtEndOfPage() && !readFillBuffer())
      return 0;
    return PgCursor.readByte();
  }

  // Note: Assumes that cursor is byte aligned when called.
  void writeByte(uint8_t Byte) {
    if (PgCursor.isIndexAtEndOfPage())
      writeFillBuffer();
    PgCursor.writeByte(Byte);
  }

  // Returns true if able to fill the buffer with at least one byte.
  bool readFillBuffer();

  // Creates new pages in buffer so that writes can occur.
  void writeFillBuffer();

  CursorImpl* copy() { return copy(Type); }

  CursorImpl* copy(StreamType Type);

  void jumpToByteAddress(size_t NewAddress);

  void freezeEof() {
    Queue->freezeEof(PgCursor.getCurAddress());
  }

  bool atEob() {
    return PgCursor.getCurAddress() >= getEobAddress() || !readFillBuffer();
  }

  size_t getEobAddress() const {
    return EobPtr->getEobAddress();
  }

  void setEobAddress(size_t NewValue) {
    EobPtr->setEobAddress(NewValue);
  }

  void pushEobAddress(size_t NewLocalEob) {
    EobPtr = std::make_shared<BlockEob>(NewLocalEob, EobPtr);
  }

  void popEobAddress() {
    EobPtr = EobPtr->getEnclosingEobPtr();
    assert(EobPtr);
  }

  StreamType getRtClassId() const { return Type; }

  static bool implementsClass(StreamType WantedType) { return true; }

  // For debugging only.
  void describe() {
    size_t EobAddress = getEobAddress();
    fprintf(stderr, "Cursor ");
    if (EobAddress != kUndefinedAddress)
      fprintf(stderr, " eob=%" PRIuMAX " ", uintmax_t(EobAddress));
    PgCursor.describe();
  }

  StreamType Type;
  // The byte queue the cursor points to.
  ByteQueue* Queue;
  // The current address into the buffer.
  PageCursor PgCursor;
  // End of block address.
  std::shared_ptr<BlockEob> EobPtr;
};

class Cursor {
  Cursor() = delete;
  Cursor& operator=(const Cursor&) = delete;

 public:
  void swap(Cursor& C) { std::swap(Impl, C.Impl); }

  StreamType getType() const { return Impl->Type; }

  size_t getCurByteAddress() const { return Impl->PgCursor.getCurAddress(); }

  void reset() {}

  ByteQueue* getQueue() { return Impl->Queue; }

  void jumpToByteAddress(size_t NewAddress) {
    Impl->jumpToByteAddress(NewAddress);
  }

  bool atEob() { return Impl->atEob(); }

  size_t getEobAddress() const { return Impl->getEobAddress(); }

  void pushEobAddress(size_t NewLocalEob) {
    Impl->pushEobAddress(NewLocalEob);
  }

  void popEobAddress() {
    Impl->popEobAddress();
  }

  void describe() { Impl->describe(); }

 protected:
  CursorImpl* Impl;

  Cursor(StreamType Type, ByteQueue* Queue)
      : Impl(new CursorImpl(Type, Queue)) {}
  explicit Cursor(const Cursor& C) : Impl(C.Impl->copy(C.Impl->Type)) {}
  ~Cursor() { delete Impl; }
};

class ReadCursor final : public Cursor {
  ReadCursor() = delete;
  ReadCursor& operator=(const ReadCursor&) = delete;

 public:
  ReadCursor(StreamType Type, ByteQueue* Queue) : Cursor(Type, Queue) {}
  explicit ReadCursor(ReadCursor& C) : Cursor(C) {}

  // Reads next byte. Returns zero if at end of buffer.
  uint8_t readByte() { return Impl->readByte(); }
};

class WriteCursor final : public Cursor {
  WriteCursor() = delete;
  WriteCursor& operator=(const WriteCursor&) = delete;

 public:
  explicit WriteCursor(StreamType Type, ByteQueue* Queue)
      : Cursor(Type, Queue) {}
  explicit WriteCursor(const WriteCursor& C) : Cursor(C) {}

  // Writes next byte. Fails if at end of buffer.
  void writeByte(uint8_t Byte) { Impl->writeByte(Byte); }

  void freezeEof() { Impl->freezeEof(); }

  // For debugging.
  void writeCurPage(FILE* File);
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_CURSOR_H
