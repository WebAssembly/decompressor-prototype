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

// Defines a (stream) writer for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_WRITER_H
#define DECOMPRESSOR_SRC_INTERP_WRITER_H

#include "interp/WriteStream.h"
#include "stream/WriteCursor.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace interp {

class Writer {
  Writer() = delete;
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

 public:
  Writer(std::shared_ptr<decode::Queue> Output, filt::TraceClassSexp& Trace);
  ~Writer() {}

  virtual decode::WriteCursor& getPos();
  virtual decode::StreamType getStreamType() const;

  virtual void reset();
  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

  // Override the following as needed. These methods return false if the writes
  // failed. Default actions are to do nothing and return true.
  virtual bool writeUint8(uint8_t Value);
  virtual bool writeUint32(uint32_t Value);
  virtual bool writeUint64(uint64_t Value);
  virtual bool writeVarint32(int32_t Value);
  virtual bool writeVarint64(int64_t Value);
  virtual bool writeVaruint32(uint32_t Value);
  virtual bool writeVaruint64(uint64_t Value);
  virtual bool writeFreezeEof();
  virtual bool writeValue(decode::IntType Value, const filt::Node* Format);
  virtual bool writeAction(const filt::CallbackNode* Action);

  virtual void describeState(FILE* File);
  filt::TraceClassSexp& getTrace() { return Trace; }

 private:
  decode::WriteCursor Pos;
  std::shared_ptr<WriteStream> Stream;
  filt::TraceClassSexp& Trace;
  bool MinimizeBlockSize;

  // The stack of block patch locations.
  decode::WriteCursor BlockStart;
  utils::ValueStack<decode::WriteCursor> BlockStartStack;
  void describeBlockStartStack(FILE* File);
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_WRITER_H
