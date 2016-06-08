#ifndef DECOMP_SRC_BYTESTREAMREADER_H
#define DECOMP_SRC_BYTESTREAMREADER_H

#include <cassert>
#include <memory>

#include "CircBuffer.h"
#include "StreamQueue.h"
#include "StreamReader.h"

namespace wasm {

namespace decode {

class ByteStreamReaderBase : public StreamReader {
  ByteStreamReaderBase(const ByteStreamReaderBase&) = delete;
  ByteStreamReaderBase &operator=(const ByteStreamReaderBase&) = delete;
 public:
  // Note: EndByte=0 implies do not know (figure out from stream).
  ByteStreamReaderBase(std::shared_ptr<CircBuffer<uint8_t>> Input,
                       size_t StartByte, size_t EndByte = 0) :
      StreamReader(StreamType::Byte), Input(Input),
      CurByte(StartByte), EndByte(EndByte) {}

  ~ByteStreamReaderBase() = default;

   bool atEof() override;

  size_t getCurByte() const {
    return CurByte;
  }

 protected:
  std::shared_ptr<CircBuffer<uint8_t>> Input;
  // Index into Input.
  size_t CurByte;
  // End of buffer, or size unknown if EndByte == 0.
  size_t EndByte;
  bool FoundEof = false;
  bool AtEof = false;
  uint8_t Buffer[64];
  // CurSize should be CurIndex, but causes clang to crash!
  size_t CurSize = 0;
  size_t BufSize = 0;

  bool jumpToByte(size_t Byte); // zero-based.

  bool getMoreBytes();
  // For bit streams, so that it can peek ahead one byte.
  bool peekOneByte(uint8_t &Byte);
};

class ByteStreamReader final : public ByteStreamReaderBase {
  ByteStreamReader(const ByteStreamReader&) = delete;
  ByteStreamReader &operator=(const ByteStreamReader&) = delete;
 public:
  // Note: EndByte=0 implies do not know (figure out from stream).
  ByteStreamReader(std::shared_ptr<CircBuffer<uint8_t>> Input,
                   size_t StartByte = 0, size_t EndByte = 0) :
      ByteStreamReaderBase(Input, StartByte, EndByte) {}

  static std::shared_ptr<ByteStreamReader>
      create(std::shared_ptr<CircBuffer<uint8_t>> Input,
             size_t StartByte = 0, size_t EndByte = 0) {
    return std::make_shared<ByteStreamReader>(Input, StartByte, EndByte);
  }

  ~ByteStreamReader() = default;

  IntType readValue() override;
  uint8_t readUint8() override;
  uint32_t readUint32() override;
  uint8_t readVaruint1() override;
  uint8_t readVaruint7() override;
  int32_t readVarint32() override;
  uint32_t readVaruint32() override;
  int64_t readVarint64() override;
  uint64_t readVaruint64() override;
  uint64_t readUint64() override;
  uint32_t readFixed32(uint32_t NumBits) override;
  uint64_t readFixed64(uint32_t NumBits) override;
  uint32_t readVbr32(uint32_t NumBits) override;
  uint64_t readVbr64(uint32_t NumBits) override;
  int32_t readIvbr32(uint32_t NumBits) override;
  int64_t readIvbr64(uint32_t NumBits) override;
  bool atEof() override;

  void jumpToByte(size_t Byte); // zero-based.
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_BYTESTREAMREADER_H
