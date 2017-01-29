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

// Defines a reader for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_READER_H
#define DECOMPRESSOR_SRC_INTERP_READER_H

#include "interp/Interpreter.def"
#include "interp/Writer.h"
#include "utils/Trace.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace filt {

class TextWriter;

}  // end of namespace filt.

namespace interp {

class Reader : public std::enable_shared_from_this<Reader> {
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

 public:
  Reader() {}
  virtual ~Reader() {}
  virtual utils::TraceClass::ContextPtr getTraceContext();
  void setTraceProgress(bool NewValue);
  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }
  virtual const char* getDefaultTraceName() const;

  virtual void reset();
  virtual void describePeekPosStack(FILE* Out) = 0;
  virtual bool canProcessMoreInputNow() = 0;
  virtual bool stillMoreInputToProcessNow() = 0;
  virtual bool atInputEof() = 0;
  virtual bool atInputEob() = 0;
  virtual void resetPeekPosStack() = 0;
  virtual void pushPeekPos() = 0;
  virtual void popPeekPos() = 0;
  virtual size_t sizePeekPosStack() = 0;
  virtual decode::StreamType getStreamType() = 0;
  virtual bool processedInputCorrectly() = 0;
  virtual bool readAction(const filt::SymbolNode* Action) = 0;
  virtual void readFillStart() = 0;
  virtual void readFillMoreInput() = 0;
  // Hard coded reads.
  virtual uint8_t readUint8() = 0;
  virtual uint32_t readUint32() = 0;
  virtual uint64_t readUint64() = 0;
  virtual int32_t readVarint32() = 0;
  virtual int64_t readVarint64() = 0;
  virtual uint32_t readVaruint32() = 0;
  virtual uint64_t readVaruint64() = 0;
  virtual bool readBinary(const filt::Node* Encoding, decode::IntType& Value);
  virtual bool readValue(const filt::Node* Format, decode::IntType& Value);
  virtual bool readHeaderValue(interp::IntTypeFormat Format,
                               decode::IntType& Value);

 protected:
  std::shared_ptr<utils::TraceClass> Trace;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_READER_H
