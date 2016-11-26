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

#ifndef DECOMPRESSOR_SRC_INTERP_INTWRITER_H
#define DECOMPRESSOR_SRC_INTERP_INTWRITER_H

#include "interp/IntStream.h"
#include "interp/Writer.h"
#include "utils/Defs.h"

#include <memory>

namespace wasm {

namespace interp {

class IntWriter : public Writer {
  IntWriter() = delete;
  IntWriter(const IntWriter&) = delete;
  IntWriter& operator=(const IntWriter&) = delete;

 public:
  IntWriter(std::shared_ptr<IntStream> Output) : Output(Output), Pos(Output) {}
  ~IntWriter() OVERRIDE {}
  void reset() OVERRIDE;
  decode::StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;

 private:
  std::shared_ptr<IntStream> Output;
  IntStream::WriteCursorWithTraceContext Pos;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTWRITER_H
