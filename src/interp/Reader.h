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

#ifndef DECOMPRESSOR_SRC_INTERP_READER_H_
#define DECOMPRESSOR_SRC_INTERP_READER_H_

#include "interp/IntFormats.h"
#include "utils/TraceAPI.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace filt {

class Node;
class SymbolNode;
class TextWriter;

}  // end of namespace filt.

namespace interp {

class Reader : public std::enable_shared_from_this<Reader> {
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

 public:
  virtual ~Reader() {}
  virtual utils::TraceContextPtr getTraceContext();
  void setTraceProgress(bool NewValue);
  bool hasTrace() { return bool(Trace); }
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
  virtual bool pushPeekPos() = 0;
  virtual bool popPeekPos() = 0;
  virtual decode::StreamType getStreamType() = 0;
  virtual bool processedInputCorrectly() = 0;
  virtual bool readAction(decode::IntType Action);
  virtual void readFillStart() = 0;
  virtual void readFillMoreInput() = 0;
  // Hard coded reads.
  virtual uint8_t readBit();
  virtual uint8_t readUint8();
  virtual uint32_t readUint32();
  virtual uint64_t readUint64();
  virtual int32_t readVarint32();
  virtual int64_t readVarint64();
  virtual uint32_t readVaruint32();
  virtual uint64_t readVaruint64() = 0;
  virtual bool alignToByte();
  virtual bool readBlockEnter();
  virtual bool readBlockExit();
  virtual bool readBinary(const filt::Node* Encoding, decode::IntType& Value);
  virtual bool readValue(const filt::Node* Format, decode::IntType& Value);
  virtual bool readHeaderValue(interp::IntTypeFormat Format,
                               decode::IntType& Value);
  // WARNING: If overridden in reader, also override in writer so that you get
  // a consistent implementation. By default, the (space) optimization defined
  // by the corresponding TableNode algorithm operator is ignored.
  virtual bool tablePush(decode::IntType Value);
  virtual bool tablePop();

 protected:
  std::shared_ptr<utils::TraceClass> Trace;
  // Defines value to return if readAction can't handle.
  const bool DefaultReadAction;
  Reader(bool DefaultReadAction) : DefaultReadAction(DefaultReadAction) {}
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_READER_H_
