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

// Base class for the internal implementation of cursors.
class CursorImpl {
  CursorImpl(const CursorImpl &) = delete;
  CursorImpl &operator=(const CursorImpl &) = delete;
  CursorImpl() = delete;
 public:

  explicit CursorImpl(StreamType Type, ByteQueue *Queue)
      : Type(Type), Queue(Queue) {}

  virtual ~CursorImpl() { releaseLock(); }

  virtual bool releaseLock();

  // Note: Assumes that cursor is byte aligned when called.
  uint8_t readByte() {
    if (Buffer == BufferEnd && !readFillBuffer())
      return 0;
    ++CurAddress;
    return *(Buffer++);
  }

  // Note: Assumes that cursor is byte aligned when called.
  void writeByte(uint8_t Byte) {
    if (Buffer == BufferEnd)
      writeFillBuffer();
    ++CurAddress;
    *(Buffer++) = Byte;
  }

  // Returns true if able to fill the buffer with at least one byte.
  bool readFillBuffer();

  // Creates new pages in buffer so that writes can occur.
  void writeFillBuffer();

  // Note: Only supported on bit cursor implementations.
  virtual uint32_t readBits(uint32_t NumBits);
  virtual void writeBits(uint8_t Value, uint32_t NumBits);

  CursorImpl *copy(StreamType Type);

  void jumpToAddress(size_t NewAddress) {
    // TODO(karlschimpf): Optimize by not throwing away lock if at least one
    // byte in buffer is still accessable.
    Queue->lock(NewAddress);
    releaseLock();
    CurAddress = NewAddress;
  }

  void freezeEob() {
    Queue->freezeEob(CurAddress);
    EobAddress = CurAddress;
  }

  StreamType getRtClassId() const { return Type; }

  static bool implementsClass(StreamType WantedType) { return true; }

  StreamType Type;
  // The byte queue the cursor points to.
  ByteQueue* Queue;
  // The current address into the buffer.
  size_t CurAddress = 0;
  // The address locked by this cursor, if Buffer != nullptr.
  // TODO(karlschimpf) Infer the locked address from the current address.
  size_t LockedAddress = 0;
  // The pointer to the locked buffer.
  uint8_t* Buffer = nullptr;
  // The pointer to the end of the locked buffer.
  uint8_t* BufferEnd = nullptr;
  // End of block address.
  size_t EobAddress = kUndefinedAddress;
};

class Cursor {
 public:

  void assign(const Cursor& C) {
    Impl->releaseLock();
  }

  void swap(Cursor& C) {
    std::swap(Impl, C.Impl);
  }

  StreamType getType() const {
    return Impl->Type;
  }

  size_t getCurAddress() const {
    return Impl->CurAddress;
  }

  void releaseLock() {
    Impl->releaseLock();
  }

  ByteQueue* getQueue() {
    return Impl->Queue;
  }

  // WARNING: Assumes that you have a lock before NewAddress when this is
  // called.
  void jumpToAddress(size_t NewAddress) {
    Impl->jumpToAddress(NewAddress);
  }

 protected:
  CursorImpl *Impl;

  Cursor(StreamType Type, ByteQueue *Queue)
      : Impl(new CursorImpl(Type, Queue)) {}
  explicit Cursor(const Cursor &C) : Impl(C.Impl->copy(C.Impl->Type))  {}
  ~Cursor() { delete Impl; }
};

class ReadCursor final : public Cursor {
 public:
  ReadCursor(StreamType Type, ByteQueue* Queue) : Cursor(Type, Queue) {}
  explicit ReadCursor(ReadCursor& C)
      : Cursor(C), LocalEobOverrides(C.LocalEobOverrides) {}

  void assign(ReadCursor& C) {
    Cursor::assign(C);
    LocalEobOverrides.swap(C.LocalEobOverrides);
  }

  size_t getEobAddress() const {
    return LocalEobOverrides.empty()
        ? Impl->EobAddress : LocalEobOverrides.back();
  }

  bool atEob() {
    size_t Eob = getEobAddress();
    return (Impl->CurAddress >= Eob)
        || !((Impl->Buffer < Impl->BufferEnd) || Impl->readFillBuffer());
  }

  // Reads next byte. Returns zero if at end of buffer.
  uint8_t readByte() {
    return Impl->readByte();
  }

  void pushEobAddress(size_t NewLocalEob) {
    LocalEobOverrides.push_back(NewLocalEob);
  }

  void popEobAddress() {
    assert(!LocalEobOverrides.empty());
    LocalEobOverrides.pop_back();
  }

 protected:
  // Stack of local Eob addresses.
  std::vector<size_t> LocalEobOverrides;
};

class WriteCursor final : public Cursor {
 public:
  explicit WriteCursor(StreamType Type, ByteQueue* Queue)
      : Cursor(Type, Queue) {}
  explicit WriteCursor(const WriteCursor& C) : Cursor(C) {}
  WriteCursor& operator=(const WriteCursor& C) {
    assign(C);
    return *this;
  }

  // Writes next byte. Fails if at end of buffer.
  void writeByte(uint8_t Byte) {
    Impl->writeByte(Byte);
  }

  void freezeEob() {
    Impl->freezeEob();
  }

  size_t getEobAddress() const {
    return Impl->EobAddress;
  }

  bool atEob() {
    return Impl->CurAddress >= Impl->EobAddress;
  }

  // For debugging.
  void writeCurPage(FILE* File);
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_CURSOR_H
