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

// Defines a stream reader for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_BYTEREADER_H
#define DECOMPRESSOR_SRC_INTERP_BYTEREADER_H

#include "interp/Reader.h"
#include "interp/ReadStream.h"
#include "stream/ReadCursor.h"

namespace wasm {

namespace interp {

class ByteReader : public InputReader {
  ByteReader() = delete;
  ByteReader(const ByteReader&) = delete;
  ByteReader& operator=(const ByteReader&) = delete;

 public:
  ByteReader(std::shared_ptr<decode::Queue> Input);
  ~ByteReader() OVERRIDE;
  void setReadPos(const decode::ReadCursor& ReadPos);
  decode::ReadCursor& getPos();

  void describePeekPosStack(FILE* Out) OVERRIDE;
  bool canProcessMoreInputNow() OVERRIDE;
  bool stillMoreInputToProcessNow() OVERRIDE;
  bool atInputEof() OVERRIDE;
  bool atInputEob() OVERRIDE;
  void resetPeekPosStack() OVERRIDE;
  void pushPeekPos() OVERRIDE;
  void popPeekPos() OVERRIDE;
  size_t sizePeekPosStack() OVERRIDE;
  decode::StreamType getStreamType() OVERRIDE;
  bool processedInputCorrectly() OVERRIDE;
  virtual bool readAction(const filt::SymbolNode* Action) OVERRIDE;
  void readFillStart() OVERRIDE;
  void readFillMoreInput() OVERRIDE;
  uint8_t readUint8() OVERRIDE;
  uint32_t readUint32() OVERRIDE;
  uint64_t readUint64() OVERRIDE;
  int32_t readVarint32() OVERRIDE;
  int64_t readVarint64() OVERRIDE;
  uint32_t readVaruint32() OVERRIDE;
  uint64_t readVaruint64() OVERRIDE;
  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;

 private:
  decode::ReadCursorWithTraceContext ReadPos;
  std::shared_ptr<ReadStream> Input;
  // The input position needed to fill to process now.
  size_t FillPos;
  // The input cursor position if back filling.
  decode::ReadCursor FillCursor;
  // The stack of read cursors (used by peek)
  decode::ReadCursor PeekPos;
  utils::ValueStack<decode::ReadCursor> PeekPosStack;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_BYTEREADER_H
