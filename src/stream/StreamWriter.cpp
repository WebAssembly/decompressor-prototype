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

#include "stream/StreamWriter.h"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>

namespace wasm {

namespace decode {

StreamWriter::~StreamWriter() {
  if (!freeze())
    fatal("Unable to close ostream!");
}

bool StreamWriter::saveBuffer() {
  if (CurSize == 0)
    return true;
  Output.write(Bytes, CurSize);
  CurSize = 0;
  if (!Output.good())
    return false;
  return true;
}

size_t StreamWriter::read(uint8_t* Buf, size_t Size) {
  (void)Buf;
  (void)Size;
  return 0;
}

bool StreamWriter::write(uint8_t* Buf, size_t Size) {
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

bool StreamWriter::freeze() {
  IsFrozen = true;
  if (!saveBuffer())
    return false;
  close();
  return true;
}

bool StreamWriter::atEof() {
  return IsFrozen;
}

FstreamWriter::FstreamWriter(const char* Filename)
    : StreamWriter(FileOutput), FileOutput(Filename) {
}

FstreamWriter::~FstreamWriter() {
}

void FstreamWriter::close() {
  if (FileOutput.is_open())
    FileOutput.close();
}

}  // end of namespace decode

}  // end of namespace wasm
