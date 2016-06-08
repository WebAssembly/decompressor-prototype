#include "ByteStreamReader.h"

#include <cassert>
#include <climits>

#include "DecodeDefs.h"

// Define a constexpr version for initializing array CeilByte.

namespace wasm {

namespace decode {

namespace {

constexpr uint32_t getBytesForN(uint32_t N) {
  return
      (N <= 8) ? 8 : ((N <= 32) ? 32 : 64);
}

uint32_t BytesForN[kBitsInIntType+1] = {
  getBytesForN(0),
  getBytesForN(1),
  getBytesForN(2),
  getBytesForN(3),
  getBytesForN(4),
  getBytesForN(5),
  getBytesForN(6),
  getBytesForN(7),
  getBytesForN(8),
  getBytesForN(9),
  getBytesForN(10),
  getBytesForN(11),
  getBytesForN(12),
  getBytesForN(13),
  getBytesForN(14),
  getBytesForN(15),
  getBytesForN(16),
  getBytesForN(17),
  getBytesForN(18),
  getBytesForN(19),
  getBytesForN(20),
  getBytesForN(21),
  getBytesForN(22),
  getBytesForN(23),
  getBytesForN(24),
  getBytesForN(25),
  getBytesForN(26),
  getBytesForN(27),
  getBytesForN(28),
  getBytesForN(29),
  getBytesForN(30),
  getBytesForN(31),
  getBytesForN(32),
  getBytesForN(33),
  getBytesForN(34),
  getBytesForN(35),
  getBytesForN(36),
  getBytesForN(37),
  getBytesForN(38),
  getBytesForN(39),
  getBytesForN(40),
  getBytesForN(41),
  getBytesForN(42),
  getBytesForN(43),
  getBytesForN(44),
  getBytesForN(45),
  getBytesForN(46),
  getBytesForN(47),
  getBytesForN(48),
  getBytesForN(49),
  getBytesForN(50),
  getBytesForN(51),
  getBytesForN(52),
  getBytesForN(53),
  getBytesForN(54),
  getBytesForN(55),
  getBytesForN(56),
  getBytesForN(57),
  getBytesForN(58),
  getBytesForN(59),
  getBytesForN(60),
  getBytesForN(61),
  getBytesForN(62),
  getBytesForN(63),
  getBytesForN(64)
};

} // end of anonymous namespace

bool ByteStreamReaderBase::jumpToByte(size_t Byte) {
  if (!Input->fill(Byte))
    return false;
  // flush buffer to force reload on next read
  CurByte = Byte;
  AtEof = false;
  CurSize = 0;
  BufSize = 0;
  return true;
}

bool ByteStreamReaderBase::getMoreBytes() {
  if (FoundEof)
    return false;
  size_t NumBytes = Input->read(CurByte, Buffer, sizeof(Buffer));
  if (EndByte) {
    if (CurByte >= EndByte) {
      NumBytes -= (CurByte - EndByte);
      FoundEof = true;
    }
  } else if (NumBytes == 0) {
    EndByte = CurByte;
    FoundEof = true;
    AtEof = true;
    return false;
  }
  CurSize = 0;
  BufSize = NumBytes;
  return NumBytes > 0;
}

bool ByteStreamReaderBase::peekOneByte(uint8_t &Byte) {
  while (true) {
    if (CurSize < BufSize) {
      Byte = Buffer[CurSize];
      return true;
    }
    if (!FoundEof) {
      getMoreBytes();
      continue;
    }
    size_t NumBytes = Input->read(CurByte, Buffer, sizeof(Buffer));
    CurSize = 0;
    BufSize = NumBytes;
    if (NumBytes == 0) {
      Byte = 0;
      return false;
    }
    Byte = Buffer[0];
    return true;
  }
}

bool ByteStreamReaderBase::atEof() {
  if (AtEof)
    return true;
  if (CurSize < BufSize)
    return false;
  AtEof = !getMoreBytes();
  return AtEof;
}

IntType ByteStreamReader::readValue() {
  return readVaruint64();
}

uint8_t ByteStreamReader::readUint8() {
  while (true) {
    if (CurSize < BufSize)
      return Buffer[CurSize++];
    if (!getMoreBytes())
      fatal("readUint8 failed, at eof");
  }
}

uint32_t ByteStreamReader::readUint32() {
  uint32_t Value = 0;
  for (size_t i = 0; i < sizeof(uint32_t); ++i)
    Value = (Value << CHAR_BIT) | readUint8();
  return Value;
}


uint8_t ByteStreamReader::readVaruint1() {
  uint8_t Value = readUint8();
  if (Value > 1)
    fatal("readVaruint1 not boolean!");
  return Value;
}

uint8_t ByteStreamReader::readVaruint7() {
  uint8_t Value = readUint8();
  if (Value > 127)
    fatal("readVaruint7 > 127!");
  return Value;
}

int32_t ByteStreamReader::readVarint32() {
  uint32_t Value = 0;
  uint32_t Shift = 0;
  while (true) {
    uint32_t Chunk = readUint8();
    uint32_t Data = Chunk & ~(static_cast<uint8_t>(1) << 7);
    Value |= Data << Shift;
    Shift += 7;
    if ((Chunk >> 7) == 0) {
      if (Shift < 32 && ((Data >> 6)  == 1))
        Value |= ~ (static_cast<uint32_t>(1) << Shift);
      return static_cast<int32_t>(Value);
    }
  }
}

uint32_t ByteStreamReader::readVaruint32() {
  uint32_t Value = 0;
  uint32_t Shift = 0;
  while (true) {
    uint32_t Chunk = readUint8();
    uint32_t Data = Chunk & ~(static_cast<uint32_t>(1) << 7);
    Value |= Data << Shift;
    if ((Chunk >> 7) == 0)
      return Value;
    Shift += 7;
  }
}

int64_t ByteStreamReader::readVarint64() {
  uint64_t Value = 0;
  uint64_t Shift = 0;
  while (true) {
    uint32_t Chunk = readUint8();
    uint32_t Data = Chunk & ~(static_cast<uint32_t>(1) << 7);
    Value |= Data << Shift;
    Shift += 7;
    if ((Chunk >> 7) == 0) {
      if (Shift < 64 && ((Data >> 6)  == 1))
        Value |= ~ (static_cast<uint64_t>(1) << Shift);
      return static_cast<int64_t>(Value);
    }
  }
}

uint64_t ByteStreamReader::readVaruint64() {
  uint64_t Value = 0;
  uint64_t Shift = 0;
  while (true) {
    uint32_t Chunk = readUint8();
    uint32_t Data = Chunk & ~(static_cast<uint32_t>(1) << 7);
    Value |= Data << Shift;
    if ((Chunk >> 7) == 0)
      return Value;
    Shift += 7;
  }
}

uint64_t ByteStreamReader::readUint64() {
  uint64_t Value = 0;
  for (size_t i = 0; i < sizeof(uint64_t); ++i)
    Value = (Value << CHAR_BIT) | readUint8();
  return Value;
}

uint32_t ByteStreamReader::readFixed32(uint32_t NumBits) {
  uint32_t Value = 0;
  assert(NumBits <= 32);
  size_t NumBytes = BytesForN[NumBits];
  while (NumBytes) {
    if (CurSize < BufSize) {
      Value = (Value << CHAR_BIT) | Buffer[CurSize++];
      --NumBytes;
      continue;
    }
    if (!getMoreBytes())
      fatal("readFixed32 failed, at eof");
  }
  return Value;
}


uint64_t ByteStreamReader::readFixed64(uint32_t NumBits) {
  uint64_t Value = 0;
  assert(NumBits <= 64);
  size_t NumBytes = BytesForN[NumBits];
  while (NumBytes) {
    if (CurSize < BufSize) {
      Value = (Value << CHAR_BIT) | Buffer[CurSize++];
      --NumBytes;
      continue;
    }
    if (!getMoreBytes())
      fatal("readFixed64 failed, at eof");
  }
  return Value;
}

uint32_t ByteStreamReader::readVbr32(uint32_t NumBits) {
  (void) NumBits;
  return readVaruint32();
}

uint64_t ByteStreamReader::readVbr64(uint32_t NumBits) {
  (void) NumBits;
  return readVaruint64();
}

int32_t ByteStreamReader::readIvbr32(uint32_t NumBits) {
  (void) NumBits;
  return readVarint32();
}

int64_t ByteStreamReader::readIvbr64(uint32_t NumBits) {
  (void) NumBits;
  return readVarint64();
}

bool ByteStreamReader::atEof() {
  if (AtEof)
    return true;
  if (CurSize < BufSize)
    return false;
  AtEof = !getMoreBytes();
  return AtEof;
}

void ByteStreamReader::jumpToByte(size_t Byte) {
  if (!ByteStreamReaderBase::jumpToByte(Byte))
    fatal("Unable to jumpToByte in ByteStreamReader!");
}

} // end of namespace decode

} // end of namespace wasm
