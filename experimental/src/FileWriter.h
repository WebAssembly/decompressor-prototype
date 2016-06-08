#ifndef DECOMP_SRC_FILE_WRITER_H
#define DECOMP_SRC_FILE_WRITER_H

#include "StreamQueue.h"

namespace wasm {

namespace decode {

class FdWriter : public StreamQueue<uint8_t> {
  FdWriter(const FdWriter&) = delete;
  FdWriter &operator=(const FdWriter&) = delete;
public:
  FdWriter(int Fd, bool CloseOnExit=true) : Fd(Fd), CloseOnExit(CloseOnExit) {}

  ~FdWriter() override;
  int read(uint8_t *Buf, size_t Size=1) override;
  bool write(uint8_t *Buf, size_t Size=1) override;
  bool freeze() override;
  bool atEof() override;

protected:
  int Fd;
  static constexpr size_t kBufSize = 4096;
  uint8_t Bytes[kBufSize];
  size_t CurSize = 0;
  bool IsFrozen = false;
  bool CloseOnExit;

  bool saveBuffer();
};

class FileWriter final : public FdWriter {
  FileWriter(const FileWriter&) = delete;
  FileWriter &operator=(const FileWriter&) = delete;
public:
  FileWriter(const char *Filename);
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_FILE_WRITER_H
