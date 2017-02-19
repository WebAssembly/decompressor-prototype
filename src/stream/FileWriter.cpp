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

#include <fcntl.h>
#include <unistd.h>

namespace wasm {

namespace decode {

FileWriter::FileWriter(const char* Filename)
    : File((strcmp(Filename, "-") == 0) ? stdout : fopen(Filename, "w")),
      CurSize(0),
      FoundErrors(false),
      IsFrozen(false),
      CloseOnExit(true) {
  if (File == nullptr) {
    FoundErrors = true;
    File = fopen("/dev/null", "w");
  }
}

FileWriter::~FileWriter() {
  if (!freeze())
    fprintf(stderr, "WARNING: Unable to close file!\n");
}

bool FileWriter::saveBuffer() {
  if (CurSize == 0)
    return true;
  size_t BufSize = CurSize;
  CurSize = 0;
  uint8_t* Buf = Bytes;
  while (BufSize) {
    size_t BytesWritten = fwrite(Buf, 1, BufSize, File);
    if (BytesWritten <= 0)
      return false;
    Buf += BytesWritten;
    BufSize -= BytesWritten;
  }
  return true;
}

size_t FileWriter::read(uint8_t* Buf, size_t Size) {
  (void)Buf;
  (void)Size;
  return 0;
}

bool FileWriter::write(uint8_t* Buf, size_t Size) {
  while (Size) {
    if (CurSize == kBufSize) {
      if (!saveBuffer())
        return false;
    }
    size_t Count = std::min(Size, kBufSize - CurSize);
    memcpy(Bytes + CurSize, Buf, Count);
    Buf += Count;
    CurSize += Count;
    Size -= Count;
  }
  return true;
}

bool FileWriter::freeze() {
  IsFrozen = true;
  if (!saveBuffer())
    return false;
  if (CloseOnExit) {
    fclose(File);
    CloseOnExit = false;
  }
  return true;
}

bool FileWriter::atEof() {
  return IsFrozen;
}

bool FileWriter::hasErrors() {
  return FoundErrors;
}

}  // end of namespace decode

}  // end of namespace wasm
