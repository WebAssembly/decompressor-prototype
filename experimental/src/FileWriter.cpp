#include "FileWriter.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "DecodeDefs.h"

namespace wasm {

namespace decode {

FdWriter::~FdWriter() {
  if (!freeze())
    fatal("Unable to close Fd file!");
}

bool FdWriter::saveBuffer() {
  if (CurSize == 0)
    return true;
  uint8_t *Buf = Bytes;
  while (CurSize) {
    size_t BytesWritten = ::write(Fd, Buf, CurSize);
    if (BytesWritten == 0)
      return false;
    Buf += BytesWritten;
    CurSize -= BytesWritten;
  }
  return true;
}

int FdWriter::read(uint8_t *Buf, size_t Size) {
  (void) Buf;
  (void) Size;
  fatal("read not defined on file writer!");
  return 0;
}

bool FdWriter::write(uint8_t *Buf, size_t Size) {
  while (Size) {
    if (CurSize == kBufSize)
      saveBuffer();
    size_t Count = std::min(Size, kBufSize - CurSize);
    memcpy(Bytes + CurSize, Buf, Count);
    CurSize += Count;
    Size -= Count;
  }
  return true;
}

bool FdWriter::freeze() {
  IsFrozen = true;
  if (!saveBuffer())
    return false;
  if (CloseOnExit) {
    close(Fd);
    CloseOnExit = false;
  }
  return true;
}

bool FdWriter::atEof() {
  fatal("atEof not defined on file writer!");
  return false;
}

FileWriter::FileWriter(const char *Filename)
    : FdWriter(open(Filename, O_WRONLY)) {}

} // end of namespace decode

} // end of namespace wasm
