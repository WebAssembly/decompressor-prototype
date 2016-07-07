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

class Cursor {
public:
  static constexpr size_t kUndefinedAddress =
      std::numeric_limits<size_t>::max();

  void assign(const Cursor& C) {
    releaseLock();
    Queue = C.Queue;
    LockedAddress = C.LockedAddress;
    Buffer = C.Buffer;
    BufferEnd = C.BufferEnd;
    CurAddress = C.CurAddress;
    EobAddress = C.EobAddress;
  }

  void swap(Cursor &C) {
    std::swap(Queue, C.Queue);
    std::swap(LockedAddress, C.LockedAddress);
    std::swap(Buffer, C.Buffer);
    std::swap(BufferEnd, C.BufferEnd);
    std::swap(CurAddress, C.CurAddress);
    std::swap(EobAddress, C.EobAddress);
  }

  size_t getEobAddress() const {
    return EobAddress;
  }

  bool atEob() const {
    return CurAddress >= EobAddress;
  }

  size_t getCurAddress() const {
    return CurAddress;
  }

  void releaseLock() {
    if (Buffer) {
      Queue->lockAddress(LockedAddress);
      Buffer = nullptr;
      BufferEnd = nullptr;
    }
  }

protected:
  // The byte queue the cursor points to.
  ByteQueue *Queue;
  // The address locked by this cursor, if Buffer != nullptr.
  size_t LockedAddress = 0;
  // The pointer to the locked buffer.
  uint8_t *Buffer = nullptr;
  // The pointer to the end of the locked buffer.
  uint8_t *BufferEnd = nullptr;
  // The current address into the buffer.
  size_t CurAddress = 0;
  // End of block address.
  size_t EobAddress = kUndefinedAddress;

  explicit Cursor (ByteQueue *Queue) : Queue(Queue) {}

  ~Cursor() {
    releaseLock();
  }

  explicit Cursor(const Cursor& C) :
      Queue(C.Queue), LockedAddress(C.LockedAddress),
      Buffer(C.Buffer), BufferEnd(C.BufferEnd), CurAddress(C.CurAddress),
      EobAddress(kUndefinedAddress) {
    // Add local copy of lock, so that lifetime matches cursor.
    if (Buffer)
      Queue->lockAddress(LockedAddress);
  }
};

class ReadCursor : public Cursor {
public:
  explicit ReadCursor(ByteQueue *Queue) : Cursor(Queue) {}
  explicit ReadCursor(const ReadCursor &C) : Cursor(C) {}
  ReadCursor &operator=(const ReadCursor &C) {
    assign(C);
    return *this;
  }

  // Reads next byte. Returns zero if at end of buffer.
  uint8_t readByte() {
    if (Buffer == BufferEnd && !fillBuffer())
      return 0;
    ++CurAddress;
    return *(Buffer++);
  }

  size_t getEobAddress() const {
    return LocalEobOverrides.empty()
        ? Cursor::getEobAddress() : LocalEobOverrides.back();
  }


  bool atEob() {
    if (BufferEnd > Buffer)
      return false;
    if (CurAddress >= EobAddress)
      return true;
    return !fillBuffer();
  }

  void pushEobAddress(size_t newLocalEob) {
    LocalEobOverrides.push_back(newLocalEob);
  }

  void popEobAddress() {
    assert(!LocalEobOverrides.empty());
    LocalEobOverrides.pop_back();
  }

protected:
  // Stack of local Eob addresses.
  std::vector<size_t> LocalEobOverrides;
  // Fills buffer with more text, if possible.
  bool fillBuffer();
};

class WriteCursor : public Cursor {
public:
  explicit WriteCursor(ByteQueue *Queue) : Cursor(Queue) {}
  explicit WriteCursor(const WriteCursor &C) : Cursor(C) {}
  WriteCursor &operator=(const WriteCursor &C) {
    assign(C);
    return *this;
  }

  // Writes next byte. Fails if at end of buffer.
  void writeByte(uint8_t Byte) {
    if (Buffer == BufferEnd)
      fillBuffer();
    ++CurAddress;
    *(Buffer++) = Byte;
  }

  void freezeEob() {
    Queue->freezeEob(CurAddress);
  }

  // For debugging.
  void writeCurPage(FILE *File);

protected:
  void fillBuffer();
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_CURSOR_H
