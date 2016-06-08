#include "FileReader.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "DecodeDefs.h"

namespace wasm {

namespace decode {

FdReader::~FdReader() {
  closeFd();
}

void FdReader::fillBuffer() {
  CurSize = ::read(Fd, Bytes, kBufSize);
  BytesRemaining = CurSize;
  if (CurSize == 0) {
    AtEof = true;
  }
}

void FdReader::closeFd() {
  if (CloseOnExit) {
    close(Fd);
    CloseOnExit = false;
  }
}

int FdReader::read(uint8_t *Buf, size_t Size) {
  size_t Count = 0;
  while (Size) {
    if (BytesRemaining >= Size) {
      const size_t Index = CurSize - BytesRemaining;
      memcpy(Buf, Bytes + Index, Size);
      BytesRemaining -= Size;
      return Count + Size;
    } else if (BytesRemaining) {
      const size_t Index = CurSize - BytesRemaining;
      memcpy(Buf, Bytes + Index, BytesRemaining);
      Buf += BytesRemaining;
      Count += BytesRemaining;
      Size -= BytesRemaining;
      BytesRemaining = 0;
    }
    if (AtEof)
      return Count;
    fillBuffer();
  }
  return Count;
}

bool FdReader::write(uint8_t *Buf, size_t Size) {
  (void) Buf;
  (void) Size;
  fatal("write not defined on file reader!");
  return false;
}

bool FdReader::freeze() {
  fatal("freeze not defined on file reader!");
  return false;
}

bool FdReader::atEof() {
  if (AtEof)
    return true;
  if (BytesRemaining)
    return false;
  fillBuffer();
  return AtEof;
}

#if 0
size_t FdReader::expandToAddress(size_t Address) {
  if (Fd < 0)
    return 0;
  // Force large reads to minimize overhead.
  constexpr size_t kChunkSize = 4096 * 4;
  if (size_t NewSize = StreamOfBufBytes::expandToAddress(
          std::max(Address, CurSize + kChunkSize))) {
    return read(Fd, &Bytes[CurSize],
                std::max(NewSize - CurSize, kChunkSize));
  }
  return 0;
}
#endif

FileReader::FileReader(const char *Filename)
    : FdReader(open(Filename, O_RDONLY), true) {}

FileReader::~FileReader() {}

} // end of namespace decode

} // end of namespace wasm
