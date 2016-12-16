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

// Defines a writer for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_WRITER_H
#define DECOMPRESSOR_SRC_INTERP_WRITER_H

#include "interp/IntFormats.h"
#include "sexp/Ast.h"

namespace wasm {

namespace interp {

class Reader;

class Writer {
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

 public:
  explicit Writer() : MinimizeBlockSize(false) {}
  virtual ~Writer();

  virtual void reset();
  virtual decode::StreamType getStreamType() const = 0;
  // Override the following as needed. These methods return false if the writes
  // failed. Default actions are to do nothing and return true.
  virtual bool writeUint8(uint8_t Value) = 0;
  virtual bool writeUint32(uint32_t Value) = 0;
  virtual bool writeUint64(uint64_t Value) = 0;
  virtual bool writeVarint32(int32_t Value) = 0;
  virtual bool writeVarint64(int64_t Value) = 0;
  virtual bool writeVaruint32(uint32_t Value) = 0;
  virtual bool writeVaruint64(uint64_t Value) = 0;
  virtual bool writeFreezeEof();
  virtual bool writeMagicNumber(uint32_t MagicNumber);
  virtual bool writeVersionNumber(uint32_t VersionNumber);
  virtual bool writeValue(decode::IntType Value, const filt::Node* Format) = 0;
  virtual bool writeTypedValue(decode::IntType Value,
                               interp::IntTypeFormat Format);
  virtual bool writeAction(const filt::CallbackNode* Action) = 0;

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }
  virtual void describeState(FILE* File);

  virtual utils::TraceClass::ContextPtr getTraceContext();
  virtual void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace);
  std::shared_ptr<filt::TraceClassSexp> getTracePtr();
  filt::TraceClassSexp& getTrace() { return *getTracePtr(); }

 protected:
  bool MinimizeBlockSize;
  std::shared_ptr<filt::TraceClassSexp> Trace;

  virtual const char* getDefaultTraceName() const;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_WRITER_H
