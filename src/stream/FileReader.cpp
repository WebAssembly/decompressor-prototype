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

#include <fcntl.h>
#include <unistd.h>

namespace wasm {

namespace decode {

FileReader::FileReader(const char* Filename)
    : File((strcmp(Filename, "-") == 0) ? stdin : fopen(Filename, "r")),
      CurSize(0),
      BytesRemaining(0),
      FoundErrors(false),
      AtEof(false),
      CloseOnExit(true) {
  if (File == nullptr || ferror(File)) {
    FoundErrors = true;
    File = fopen("/dev/null", "r");
  }
}

FileReader::~FileReader() {
  closeFile();
}

bool FileReader::hasErrors() {
  return FoundErrors;
}

void FileReader::fillBuffer() {
  CurSize = fread(Bytes, sizeof(ByteType), kBufSize, File);
  BytesRemaining = CurSize;
  if (CurSize < kBufSize) {
    if (ferror(File)) {
      FoundErrors = true;
      AtEof = true;
    }
    if (CurSize == 0 && feof(File))
      AtEof = true;
  }
}

void FileReader::closeFile() {
  if (CloseOnExit) {
    fclose(File);
    CloseOnExit = false;
    File = nullptr;
  }
}

AddressType FileReader::read(ByteType* Buf, AddressType Size) {
  AddressType Count = 0;
  while (Size) {
    if (BytesRemaining >= Size) {
      const AddressType Index = CurSize - BytesRemaining;
      memcpy(Buf, Bytes + Index, Size);
      BytesRemaining -= Size;
      return Count + Size;
    } else if (BytesRemaining) {
      const AddressType Index = CurSize - BytesRemaining;
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

bool FileReader::write(ByteType* Buf, AddressType Size) {
  (void)Buf;
  (void)Size;
  return false;
}

bool FileReader::freeze() {
  closeFile();
  return false;
}

bool FileReader::atEof() {
  if (AtEof)
    return true;
  if (BytesRemaining)
    return false;
  fillBuffer();
  return AtEof;
}

}  // end of namespace decode

}  // end of namespace wasm
