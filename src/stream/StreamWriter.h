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

#ifndef DECOMPRESSOR_SRC_STREAM_STREAMWRITER_H
#define DECOMPRESSOR_SRC_STREAM_STREAMWRITER_H

#include "stream/RawStream.h"

#include <fstream>
#include <ostream>
#include <memory>

namespace wasm {

namespace decode {

class StreamWriter : public RawStream {
  StreamWriter(const StreamWriter&) = delete;
  StreamWriter& operator=(const StreamWriter&) = delete;

 public:
  StreamWriter(std::ostream& Output)
      : Output(Output), CurSize(0), IsFrozen(false) {}

  ~StreamWriter() OVERRIDE;
  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;
  bool hasErrors() OVERRIDE;

 protected:
  std::ostream& Output;
  static constexpr size_t kBufSize = 2;  // 4096;
  char Bytes[kBufSize];
  size_t CurSize;
  bool IsFrozen;

  bool saveBuffer();
  virtual void close() {}
};

class FstreamWriter FINAL : public StreamWriter {
  FstreamWriter(const FstreamWriter&) = delete;
  FstreamWriter& operator=(const FstreamWriter&) = delete;

 public:
  FstreamWriter(const char* Filename);
  ~FstreamWriter() OVERRIDE;

 private:
  std::ofstream FileOutput;
  void close() OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_STREAMWRITER_H
