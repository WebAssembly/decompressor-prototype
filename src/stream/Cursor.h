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

#include "stream/Queue.h"

namespace wasm {

namespace decode {

class Cursor;
// Holds bits in incomplete byte read/write.
class WorkingByte {
 public:
  WorkingByte() : Value(0), BitsInValue(0) {}

  uint8_t getValue() const { return Value; }

  bool isEmpty() const { return BitsInValue == 0; }

  BitsInByteType getBitsRead() const {
    return (CHAR_BIT - BitsInValue) & 0x7;
  }

  BitsInByteType getReadBitsRemaining() const {
    return BitsInValue;
  }

  BitsInByteType getBitsWritten() const {
    return BitsInValue;
  }

  BitsInByteType getWriteBitsRemaining() const {
    return CHAR_BIT - BitsInValue;
  }

  uint8_t readBits(BitsInByteType NumBits) {
    assert(NumBits <= BitsInValue);
    uint8_t Result = Value >> (BitsInValue - NumBits);
    BitsInValue -= NumBits;
    Value &= ~uint32_t(0) << BitsInValue;
    return Result;
  }


  void writeBits(uint8_t Value, BitsInByteType NumBits) {
    assert(NumBits <= BitsInValue);
    Value = (Value << NumBits) | Value;
    BitsInValue += NumBits;
  }

  void setByte(uint8_t Byte) {
    Value = Byte;
    BitsInValue = CHAR_BIT;
  }

  void reset() {
    Value = 0;
    BitsInValue = 0;
  }

  // For debugging.
  FILE* describe(FILE* File);

 private:
  uint8_t Value;
  BitsInByteType BitsInValue;
};

class Cursor : public PageCursor {
  Cursor() = delete;
  Cursor& operator=(const Cursor&) = delete;

 public:

  Cursor(StreamType Type, std::shared_ptr<Queue> Que)
      : Type(Type), Que(Que), EobPtr(Que->getEofPtr()) {}
  explicit Cursor(const Cursor& C)
      : PageCursor(C),
        Type(C.Type),
        Que(C.Que),
        EobPtr(C.EobPtr),
        CurByte(C.CurByte) {}
  ~Cursor() {}

  void swap(Cursor& C) {
    std::swap(Type, C.Type);
    std::swap(Que, C.Que);
    std::swap(static_cast<PageCursor&>(*this), static_cast<PageCursor&>(C));
    std::swap(EobPtr, C.EobPtr);
    std::swap(CurByte, C.CurByte);
  }

  StreamType getType() const { return Type; }

  void reset() {}

  bool isByteAligned() const { return CurByte.isEmpty(); }

  const WorkingByte& getWorkingByte() const { return CurByte; }

  BitsInByteType getBitsRead() const {
    return CurByte.getBitsRead();
  }

  BitsInByteType getBitsWritten() const {
    return CurByte.getBitsWritten();
  }

  std::shared_ptr<Queue> getQueue() { return Que; }

  bool atReadBitEob() {
    return getCurReadBitAddress() == getEobAddress();
  }

  bool atByteEob() {
    return getCurAddress() >= getEobAddress().getByteAddress()
        || !readFillBuffer();
  }

  BitAddress& getEobAddress() const { return EobPtr->getEobAddress(); }

  void freezeEof() { Que->freezeEof(getCurAddress()); }

  void setEobAddress(const BitAddress& NewValue) {
    EobPtr->setEobAddress(NewValue);
  }

  void pushEobAddress(const BitAddress& NewValue) {
    EobPtr = std::make_shared<BlockEob>(NewValue, EobPtr);
  }

  void popEobAddress() {
    EobPtr = EobPtr->getEnclosingEobPtr();
    assert(EobPtr);
  }

  // ------------------------------------------------------------------------
  // The following methods assume that the cursor is accessing a byte stream.
  // ------------------------------------------------------------------------

  size_t getCurByteAddress() const {
    assert(isByteAligned());
    return getCurAddress();
  }

  void jumpToByteAddress(size_t NewAddress);

  // Reads next byte. Returns zero if at end of file.
  uint8_t readByte() {
    assert(isByteAligned());
    if (isIndexAtEndOfPage() && !readFillBuffer())
      return 0;
    return PageCursor::readByte();
  }

  // Writes next byte. Fails if at end of file.
  void writeByte(uint8_t Byte) {
    assert(isByteAligned());
    if (isIndexAtEndOfPage())
      writeFillBuffer();
    PageCursor::writeByte(Byte);
  }

  // ------------------------------------------------------------------------
  // The following methods assume that the cursor is accessing a bit stream.
  // ------------------------------------------------------------------------

  BitAddress getCurReadBitAddress() const {
    BitAddress Address(getCurAddress(), getBitsRead());
    return Address;
  }

  BitAddress getCurWriteBitAddress() const {
    BitAddress Address(getCurAddress(), getBitsWritten());
    return Address;
  }

  // Reads up to 32 bits from the input.
  uint32_t readBits(uint32_t NumBits);

  // Writes up to 32 bits to the output.
  void writeBits(uint32_t Value, uint32_t NumBits);

  // ------------------------------------------------------------------------
  // The following methods are for debugging.
  // ------------------------------------------------------------------------

  void writeCurPage(FILE* File) { Que->writePageAt(File, getCurAddress()); }

  // For debugging.
  FILE* describe(FILE* File, bool IncludeDetail = false);

 protected:
  StreamType Type;
  // The byte queue the cursor points to.
  std::shared_ptr<Queue> Que;
  // End of block address.
  std::shared_ptr<BlockEob> EobPtr;
  WorkingByte CurByte;

  // Returns true if able to fill the buffer with at least one byte.
  bool readFillBuffer();

  // Creates new pages in buffer so that writes can occur.
  void writeFillBuffer();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_CURSOR_H
