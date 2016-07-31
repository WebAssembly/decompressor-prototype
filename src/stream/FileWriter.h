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

// Writes text into a file descriptor.

#ifndef DECOMPRESSOR_SRC_STREAM_FILEWRITER_H
#define DECOMPRESSOR_SRC_STREAM_FILEWRITER_H

#include "stream/RawStream.h"

#include <memory>

namespace wasm {

namespace decode {

class FdWriter : public RawStream {
  FdWriter(const FdWriter&) = delete;
  FdWriter& operator=(const FdWriter&) = delete;

 public:
  FdWriter(int Fd, bool CloseOnExit = true)
      : Fd(Fd), CurSize(0), IsFrozen(false), CloseOnExit(CloseOnExit) {}

  ~FdWriter() OVERRIDE;
  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;

  static std::unique_ptr<RawStream> create(int Fd, bool CloseOnExit = true) {
    // TODO(kschimpf): Can we make the shared pointr part of the writer?
    std::unique_ptr<RawStream> Writer(new FdWriter(Fd, CloseOnExit));
    return Writer;
  }

 protected:
  int Fd;
  static constexpr size_t kBufSize = 4096;
  uint8_t Bytes[kBufSize];
  size_t CurSize;
  bool IsFrozen;
  bool CloseOnExit;

  bool saveBuffer();
};

class FileWriter FINAL : public FdWriter {
  FileWriter(const FileWriter&) = delete;
  FileWriter& operator=(const FileWriter&) = delete;

 public:
  FileWriter(const char* Filename);
  ~FileWriter() OVERRIDE;

  static std::unique_ptr<RawStream> create(const char* Filename) {
    // TODO(kschimpf): Can we make the shared pointr part of the writer?
    std::unique_ptr<RawStream> Writer(new FileWriter(Filename));
    return Writer;
  }
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_FILEWRITER_H
