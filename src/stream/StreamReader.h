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

#ifndef DECOMPRESSOR_SRC_STREAM_STREAMREADER_H
#define DECOMPRESSOR_SRC_STREAM_STREAMREADER_H

#include "stream/RawStream.h"

#include <istream>
#include <fstream>
#include <memory>

namespace wasm {

namespace decode {

class StreamReader : public RawStream {
  StreamReader(const StreamReader&) = delete;
  StreamReader& operator=(const StreamReader*) = delete;

 public:
  StreamReader(std::istream& Input);
  ~StreamReader() OVERRIDE;
  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;
  bool hasErrors() OVERRIDE;

 protected:
  std::istream& Input;
  static constexpr size_t kBufSize = 4096;
  char Bytes[kBufSize];
  size_t CurSize;
  size_t BytesRemaining;
  bool AtEof;

  virtual void close() {}
  void fillBuffer();
};

// Defines a file reader.
class FstreamReader FINAL : public StreamReader {
  FstreamReader(const FstreamReader&) = delete;
  FstreamReader& operator=(const FstreamReader&) = delete;

 public:
  FstreamReader(const char* Filename);
  ~FstreamReader() OVERRIDE;

 private:
  std::ifstream FileInput;
  void close() OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_STREAMREADER_H
