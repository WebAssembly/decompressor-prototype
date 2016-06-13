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

// Reads text from a file descriptor.

#ifndef DECOMPRESSOR_SRC_STREAM_FILEREADER_H
#define DECOMPRESSOR_SRC_STREAM_FILEREADER_H

#include "stream/RawStream.h"

#include <memory>

namespace wasm {

namespace decode {

class FdReader : public RawStream {
  FdReader(const FdReader&) = delete;
  FdReader &operator=(const FdReader*) = delete;
public:
  ~FdReader() override;
  size_t read(uint8_t *Buf, size_t Size=1) override;
  bool write(uint8_t *Buf, size_t Size=1) override;
  bool freeze() override;
  bool atEof() override;

  static std::unique_ptr<RawStream> create(int Fd, bool CloseOnExit=true) {
    // TODO(kschimpf): Can we make the shared pointer part of the reader?
    std::unique_ptr<RawStream> Reader(new FdReader(Fd, CloseOnExit));
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

// Defines a file reader.
class FileReader final : public FdReader {
  FileReader(const FileReader&) = delete;
  FileReader &operator=(const FileReader&) = delete;
public:
  FileReader(const char *Filename);
  ~FileReader() override;
  static std::unique_ptr<RawStream> create(const char *Filename) {
    // TODO(kschimpf): Can we make the shared pointer part of the reader?
    std::unique_ptr<RawStream> Reader(new FileReader(Filename));
    return Reader;
  }
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_FILEREADER_H
