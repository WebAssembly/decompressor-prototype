#include "BitStreamReader.h"

#include <cassert>
#include <climits>
#if 0

#include "defs.h"
#endif

namespace wasm {

namespace decode {

BitStreamReader::BitStreamReader(std::shared_ptr<CircBuffer<uint8_t>> Input,
                                 size_t StartBit, size_t EndBit) :
    ByteStreamReaderBase(Input, Utils::floorByte(StartBit),
                         Utils::floorByte(EndBit)) {
  // Note: We set EndByte too early. We do this to speed up the logic of
  // getNextWord. It only needs to do special handling once EndByte is reached.
  StartOffset = StartBit - CurByte * CHAR_BIT;
  EndOffset = EndBit - EndByte * CHAR_BIT;
  PeekedAtEnd = (EndOffset == 0);
}

IntType BitStreamReader::readValue() {
  return readIvbr64(6);
}

uint8_t BitStreamReader::readUint8() {
  return readFixed32(8);
}

uint32_t BitStreamReader::readUint32() {
  return readFixed32(32);
}

uint8_t BitStreamReader::readVaruint1() {
  return readFixed32(1);
}

uint8_t BitStreamReader::readVaruint7() {
  return readFixed32(7);
}

int32_t BitStreamReader::readVarint32() {
  return readIvbr32(6);
}

uint32_t BitStreamReader::readVaruint32() {
  return readVbr32(6);
}

int64_t BitStreamReader::readVarint64() {
  return readIvbr64(6);
}

uint64_t BitStreamReader::readVaruint64()  {
  return readIvbr64(6);
}

uint64_t BitStreamReader::readUint64() {
  return readFixed64(64);
}

uint32_t BitStreamReader::readFixed32(uint32_t NumBits) {
  uint32_t Value = 0;
  assert(NumBits <= 32);
  while (NumBits) {
    if (BitsRemaining) {
      if (BitsRemaining >= NumBits) {
        uint32_t Bits = BitsRemaining - NumBits;
        Value = (Value << NumBits) | (CurBits >> Bits);
        CurBits &= (1 << Bits) - 1;
        BitsRemaining -= NumBits;
        return Value;
      }
      NumBits -= BitsRemaining;
      Value = (Value << BitsRemaining) | CurBits;
      CurBits = 0;
      BitsRemaining = 0;
    }
    if (CurSize < BufSize) {
      CurBits = Buffer[CurSize++];
      BitsRemaining = CHAR_BIT;
    } else if (!getMoreBits()) {
      fatal("readFixed failed, at eof");
    }
  }
  return Value;
}

uint64_t BitStreamReader::readFixed64(uint32_t NumBits) {
  uint64_t Value = 0;
  assert(NumBits <= 64);
  while (NumBits) {
    if (BitsRemaining) {
      if (BitsRemaining >= NumBits) {
        uint32_t Bits = BitsRemaining - NumBits;
        Value = (Value << NumBits) | (CurBits >> Bits);
        CurBits &= (1 << Bits) - 1;
        BitsRemaining -= NumBits;
        return Value;
      }
      NumBits -= BitsRemaining;
      Value = (Value << BitsRemaining) | CurBits;
      CurBits = 0;
      BitsRemaining = 0;
    }
    if (CurSize < BufSize) {
      CurBits = Buffer[CurSize++];
      BitsRemaining = CHAR_BIT;
    } else if (!getMoreBits()) {
      fatal("readFixed failed, at eof");
    }
  }
  return Value;
}

uint32_t BitStreamReader::readVbr32(uint32_t NumBits) {
  assert(1 < NumBits);
  uint32_t Value = 0;
  uint32_t Shift = 0;
  const uint32_t NumBitsM1 = NumBits - 1;
  while (true) {
    uint32_t Chunk = readFixed32(NumBits);
    uint32_t Data = Chunk & ~(static_cast<uint32_t>(1) << NumBitsM1);
    Value |= Data << Shift;
    if ((Chunk >> NumBitsM1) == 0)
      return Value;
    Shift += NumBitsM1;
  }
}

uint64_t BitStreamReader::readVbr64(uint32_t NumBits) {
  assert(1 < NumBits);
  uint64_t Value = 0;
  uint64_t Shift = 0;
  const uint32_t NumBitsM1 = NumBits - 1;
  while (true) {
    uint64_t Chunk = readFixed64(NumBits);
    uint64_t Data = Chunk & ~(static_cast<uint64_t>(1) << NumBitsM1);
    Value |= Data << Shift;
    if ((Chunk >> NumBitsM1) == 0)
      return Value;
    Shift += NumBitsM1;
  }
}

int32_t BitStreamReader::readIvbr32(uint32_t NumBits) {
  assert(1 < NumBits);
  uint32_t Value = 0;
  uint32_t Shift = 0;
  const uint32_t NumBitsM1 = NumBits - 1;
  while (true) {
    uint32_t Chunk = readFixed32(NumBits);
    uint32_t Data = Chunk & ~(static_cast<uint8_t>(1) << NumBitsM1);
    Value |= Data << Shift;
    Shift += NumBitsM1;
    if ((Chunk >> NumBitsM1) == 0) {
      if (Shift < 32 && ((Data >> (NumBitsM1 - 1))  == 1))
        Value |= ~ (static_cast<uint32_t>(1) << Shift);
      return static_cast<int32_t>(Value);
    }
  }
}

int64_t BitStreamReader::readIvbr64(uint32_t NumBits) {
  uint64_t Value = 0;
  uint64_t Shift = 0;
  const uint32_t NumBitsM1 = NumBits - 1;
  while (true) {
    uint64_t Chunk = readFixed64(NumBits);
    uint64_t Data = Chunk & ~(static_cast<uint64_t>(1) << NumBitsM1);
    Value |= Data << Shift;
    Shift += NumBitsM1;
    if ((Chunk >> NumBitsM1) == 0) {
      if (Shift < 64 && ((Data >> (NumBitsM1 - 1))  == 1))
        Value |= ~ (static_cast<uint64_t>(1) << Shift);
      return static_cast<int64_t>(Value);
    }
  }
}

bool BitStreamReader::atEof() {
  if (AtEof)
    return true;
  while (BitsRemaining == 0) {
    if (CurSize < BufSize)
      return false;
    if (!getMoreBits())
      break;
  }
  AtEof = true;
  return AtEof;
}

bool BitStreamReader::getMoreBits() {
  if (getMoreBytes())
    return true;
  // Fill in last word if applicable.
  if (PeekedAtEnd)
    return false;
  uint8_t PeekByte;
  if (peekOneByte(PeekByte)) {
    PeekedAtEnd = true;
    CurBits = PeekByte >> (CHAR_BIT - EndOffset);
    BitsRemaining = EndOffset;
    return BitsRemaining > 0;
  }
  return false;
}

void BitStreamReader::jumpToBit(size_t Bit) {
  size_t Byte = Utils::floorByte(Bit);
  jumpToByte(Byte);
  CurBits = 0;
  BitsRemaining = 0;
  PeekedAtEnd = false;
  AtEof = false;
  Bit = Bit - (Byte * CHAR_BIT);
  if (Bit)
    readFixed32(Bit);
  return;
}

} // end of namespace decode

} // end of namespace wasm
