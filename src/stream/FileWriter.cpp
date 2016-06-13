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

#include "stream/FileWriter.h"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>

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
    ssize_t BytesWritten = ::write(Fd, Buf, CurSize);
    if (BytesWritten <= 0)
      return false;
    Buf += BytesWritten;
    CurSize -= BytesWritten;
  }
  return true;
}

size_t FdWriter::read(uint8_t *Buf, size_t Size) {
  (void) Buf;
  (void) Size;
  return 0;
}

bool FdWriter::write(uint8_t *Buf, size_t Size) {
  while (Size) {
    if (CurSize == kBufSize) {
      if (!saveBuffer())
        return false;
    }
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
  return IsFrozen;
}

FileWriter::FileWriter(const char *Filename)
    : FdWriter(open(Filename, O_WRONLY | O_CREAT, S_IRWXU)) {}

FileWriter::~FileWriter() {}

} // end of namespace decode

} // end of namespace wasm
