#ifndef DECOMP_SRC_STREAMREADER_H
#define DECOMP_SRC_STREAMREADER_H

#include "DecodeDefs.h"

namespace wasm {

namespace decode {

class StreamReader {
  StreamReader(const StreamReader&) = delete;
  StreamReader &operator=(const StreamReader&) = delete;
public:
  StreamReader(StreamType Type) : Type(Type) {}

  StreamType getType() const { return Type; }

  virtual IntType readValue() = 0;

  virtual uint8_t readUint8() = 0;

  virtual uint32_t readUint32() = 0;

  virtual uint8_t readVaruint1() = 0;

  virtual uint8_t readVaruint7() = 0;

  virtual int32_t readVarint32() = 0;

  virtual uint32_t readVaruint32() = 0;

  virtual int64_t readVarint64() = 0;

  virtual uint64_t readVaruint64() = 0;

  virtual uint64_t readUint64() = 0;

  virtual uint32_t readFixed32(uint32_t Size) = 0;

  virtual uint64_t readFixed64(uint32_t Size) = 0;

  virtual uint32_t readVbr32(uint32_t Size) = 0;

  virtual uint64_t readVbr64(uint32_t Size) = 0;

  virtual int32_t readIvbr32(uint32_t Size) = 0;

  virtual int64_t readIvbr64(uint32_t Size) = 0;

  virtual bool atEof() = 0;

 private:
  StreamType Type;
};

} // end of namespace decodde

} // end of namespace wasm

#endif // DECOMP_SRC_STREAMREADER_H
