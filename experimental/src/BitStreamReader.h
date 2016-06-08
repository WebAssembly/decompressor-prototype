#ifndef DECOMP_SRC_BITSTREAMREADER_H
#define DECOMP_SRC_BITSTREAMREADER_H

#include "ByteStreamReader.h"

namespace wasm {

namespace decode {

class BitStreamReader final : public ByteStreamReaderBase {
  BitStreamReader(const BitStreamReader&) = delete;
  BitStreamReader &operator=(const BitStreamReader&) = delete;
 public:
  // Note: EndBit=0 implies do not know (figure out from stream).
  // Note: Doesn't do any reads.
  BitStreamReader(std::shared_ptr<CircBuffer<uint8_t>> Input,
                  size_t StartBit, size_t EndBit = 0);

  static std::shared_ptr<BitStreamReader>
      create(std::shared_ptr<CircBuffer<uint8_t>> Input,
             size_t StartBit = 0, size_t EndBit = 0) {
    return std::make_shared<BitStreamReader>(Input, StartBit, EndBit);
  }

  ~BitStreamReader() = default;

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

  size_t getCurBit() const {
    return CurByte * CHAR_BIT + (CHAR_BIT - BitsRemaining);
  }

  void jumpToBit(size_t Bit);

 private:
  uint8_t CurBits = 0;
  uint8_t BitsRemaining = 0;
  bool PeekedAtEnd;
  // bool AtEof = false;

  uint32_t StartOffset;
  uint32_t EndOffset;

  bool getMoreBits();
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_BITSTREAMREADER_H
