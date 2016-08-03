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

#include "stream/StreamReader.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace wasm {

namespace decode {

StreamReader::StreamReader(std::istream& Input)
    : Input(Input), CurSize(0), BytesRemaining(0), AtEof(false) {
}

StreamReader::~StreamReader() {
  close();
}

void StreamReader::fillBuffer() {
  CurSize = Input.readsome(Bytes, kBufSize);
  if (CurSize == 0) {
    // Can't read, no buffered characters. Force hard read.
    for (size_t i = 0; i < kBufSize; ++i) {
      int C = Input.get();
      if (!Input.good())
        break;
      Bytes[CurSize++] = C;
    }
  }
  BytesRemaining = CurSize;
}

size_t StreamReader::read(uint8_t* Buf, size_t Size) {
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
    if (!Input.good()) {
      AtEof = true;
      return Count;
    }
    fillBuffer();
  }
  return Count;
}

bool StreamReader::write(uint8_t* Buf, size_t Size) {
  (void)Buf;
  (void)Size;
  return false;
}

bool StreamReader::freeze() {
  // Assume that file should be truncated at current location.
  CurSize -= BytesRemaining;
  close();
  AtEof = true;
  return true;
}

bool StreamReader::atEof() {
  if (AtEof)
    return true;
  if (BytesRemaining)
    return false;
  fillBuffer();
  return AtEof;
}

FstreamReader::FstreamReader(const char* Filename)
    : StreamReader(FileInput), FileInput(Filename) {
}

FstreamReader::~FstreamReader() {
}

void FstreamReader::close() {
  if (FileInput.is_open())
    FileInput.close();
}

}  // end of namespace decode

}  // end of namespace wasm
