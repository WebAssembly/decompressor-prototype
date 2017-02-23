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

// Defines a writer for a (non-file based) integer stream.

#ifndef DECOMPRESSOR_SRC_INTERP_INTWRITER_H_
#define DECOMPRESSOR_SRC_INTERP_INTWRITER_H_

#include "interp/IntStream.h"
#include "interp/Writer.h"

namespace wasm {

namespace interp {

class IntWriter : public Writer {
  IntWriter() = delete;
  IntWriter(const IntWriter&) = delete;
  IntWriter& operator=(const IntWriter&) = delete;

 public:
  IntWriter(std::shared_ptr<IntStream> Output);
  ~IntWriter() OVERRIDE {}
  void reset() OVERRIDE;
  decode::StreamType getStreamType() const OVERRIDE;
  bool write(decode::IntType Value) { return Pos.write(Value); }
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeBlockEnter() OVERRIDE;
  bool writeBlockExit() OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  utils::TraceContextPtr getTraceContext() OVERRIDE;
  void describeState(FILE* File) OVERRIDE;
  size_t getIndex() const { return Pos.getIndex(); }

 private:
  std::shared_ptr<IntStream> Output;
  IntStream::WriteCursor Pos;

  const char* getDefaultTraceName() const OVERRIDE;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTWRITER_H_
