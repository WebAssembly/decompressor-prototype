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

#include "stream/FileReader.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

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

size_t FdReader::read(uint8_t *Buf, size_t Size) {
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
  return false;
}

bool FdReader::freeze() {
  // Assume that file should be truncated at current location.
  CurSize -= BytesRemaining;
  AtEof = true;
  return true;
}

bool FdReader::atEof() {
  if (AtEof)
    return true;
  if (BytesRemaining)
    return false;
  fillBuffer();
  return AtEof;
}

FileReader::FileReader(const char *Filename)
    : FdReader(open(Filename, O_RDONLY), true) {}

FileReader::~FileReader() {}

} // end of namespace decode

} // end of namespace wasm
