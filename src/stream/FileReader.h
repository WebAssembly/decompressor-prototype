// -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Reads text from a file descriptor.

#ifndef DECOMPRESSOR_SRC_STREAM_FILEREADER_H
#define DECOMPRESSOR_SRC_STREAM_FILEREADER_H

#include "stream/RawStream.h"

#include <cstdio>
#include <memory>

namespace wasm {

namespace decode {

#if 0
class FdReader : public RawStream {
  FdReader() = delete;
  FdReader(const FdReader&) = delete;
  FdReader& operator=(const FdReader*) = delete;

 public:
  FdReader(int Fd, bool CloseOnExit)
      : Fd(Fd),
        CurSize(0),
        BytesRemaining(0),
        FoundErrors(Fd < 0),
        AtEof(false),
        CloseOnExit(CloseOnExit) {}

  ~FdReader() OVERRIDE;

  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;
  bool hasErrors() OVERRIDE;

 protected:
  int Fd;
  static constexpr size_t kBufSize = 4096;
  uint8_t Bytes[kBufSize];
  size_t CurSize;
  size_t BytesRemaining;
  bool FoundErrors;
  bool AtEof;
  bool CloseOnExit;

  void closeFd();
  void fillBuffer();
};

// Defines a file reader.
class FileReader FINAL : public FdReader {
  FileReader(const FileReader&) = delete;
  FileReader& operator=(const FileReader&) = delete;

 public:
  FileReader(const char* Filename);
  ~FileReader() OVERRIDE;
};

#else

class FileReader : public RawStream {
  FileReader() = delete;
  FileReader(const FileReader&) = delete;
  FileReader& operator=(const FileReader*) = delete;

 public:
  FileReader(FILE* File, bool CloseOnExit)
      : File(File),
        CurSize(0),
        BytesRemaining(0),
        FoundErrors(false),
        AtEof(false),
        CloseOnExit(CloseOnExit) {}

  FileReader(const char*);

  ~FileReader() OVERRIDE;

  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;
  bool hasErrors() OVERRIDE;

 protected:
  FILE* File;
  static constexpr size_t kBufSize = 4096;
  uint8_t Bytes[kBufSize];
  size_t CurSize;
  size_t BytesRemaining;
  bool FoundErrors;
  bool AtEof;
  bool CloseOnExit;

  void closeFile();
  void fillBuffer();
};

#endif

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_FILEREADER_H
