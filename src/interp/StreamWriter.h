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

// Defines a stream writer for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_STREAMWRITER_H
#define DECOMPRESSOR_SRC_INTERP_STREAMWRITER_H

#include "interp/Writer.h"
#include "interp/WriteStream.h"
#include "stream/WriteCursor.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace interp {

class StreamWriter : public Writer {
  StreamWriter() = delete;
  StreamWriter(const StreamWriter&) = delete;
  StreamWriter& operator=(const StreamWriter&) = delete;

 public:
  StreamWriter(std::shared_ptr<decode::Queue> Output);
  ~StreamWriter() OVERRIDE;
  decode::WriteCursor& getPos();
  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;

  void setPos(const decode::WriteCursor& NewPos) { Pos = NewPos; }

  const decode::WriteCursor& getWritePos() const { return Pos; }

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
  bool writeAction(const filt::SymbolNode* Action) OVERRIDE;

  void describeState(FILE* File) OVERRIDE;

 private:
  decode::WriteCursorWithTraceContext Pos;
  std::shared_ptr<WriteStream> Stream;
  // The stack of block patch locations.
  decode::WriteCursor BlockStart;
  utils::ValueStack<decode::WriteCursor> BlockStartStack;
  void describeBlockStartStack(FILE* File);
  const char* getDefaultTraceName() const OVERRIDE;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_STREAMWRITER_H
