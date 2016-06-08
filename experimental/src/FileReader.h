#ifndef DECOMP_SRC_FILEREADER_H
#define DECOMP_SRC_FILEREADER_H

#include "StreamQueue.h"

#include <memory>

namespace wasm {

namespace decode {

class FdReader : public StreamQueue<uint8_t> {
  FdReader(const FdReader&) = delete;
  FdReader &operator=(const FdReader*) = delete;
public:

  ~FdReader() override;
  int read(uint8_t *Buf, size_t Size=1) override;
  bool write(uint8_t *Buf, size_t Size=1) override;
  bool freeze() override;
  bool atEof() override;

  static std::unique_ptr<FdReader> create(int Fd, bool CloseOnExit=true) {
    std::unique_ptr<FdReader> Reader(new FdReader(Fd, CloseOnExit));
    return Reader;
  }

protected:
  int Fd;
  static constexpr size_t kBufSize = 4096;
  uint8_t Bytes[kBufSize];
  size_t CurSize = 0;
  size_t BytesRemaining = 0;
  bool AtEof = false;
  bool CloseOnExit;

  FdReader(int Fd, bool CloseOnExit) : Fd(Fd), CloseOnExit(CloseOnExit) {}
  void closeFd();
  void fillBuffer();
};

class FileReader final : public FdReader {
  FileReader(const FileReader&) = delete;
  FileReader &operator=(const FileReader&) = delete;
public:
  FileReader(const char *Filename);
  ~FileReader() override;
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_FILEREADER_H
