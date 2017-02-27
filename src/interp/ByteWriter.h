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

#ifndef DECOMPRESSOR_SRC_INTERP_BYTEWRITER_H
#define DECOMPRESSOR_SRC_INTERP_BYTEWRITER_H

#include <map>

#include "interp/Writer.h"
#include "stream/BitWriteCursor.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace interp {

class WriteStream;

class ByteWriter : public Writer {
  ByteWriter() = delete;
  ByteWriter(const ByteWriter&) = delete;
  ByteWriter& operator=(const ByteWriter&) = delete;

 public:
  ByteWriter(std::shared_ptr<decode::Queue> Output);
  ~ByteWriter() OVERRIDE;
  utils::TraceContextPtr getTraceContext() OVERRIDE;

  decode::BitWriteCursor& getPos();
  void setPos(const decode::BitWriteCursor& NewPos);
  void reset() OVERRIDE;
  decode::StreamType getStreamType() const OVERRIDE;
  bool writeBit(uint8_t Value) OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool alignToByte() OVERRIDE;
  bool writeBlockEnter() OVERRIDE;
  bool writeBlockExit() OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeBinary(decode::IntType, const filt::Node* Encoding) OVERRIDE;
  bool tablePush(decode::IntType Value) OVERRIDE;
  bool tablePop() OVERRIDE;

  void describeState(FILE* File) OVERRIDE;

 private:
  decode::BitWriteCursor WritePos;
  std::shared_ptr<WriteStream> Stream;
  // The stack of block patch locations.
  decode::BitWriteCursor BlockStart;
  utils::ValueStack<decode::BitWriteCursor> BlockStartStack;
  void describeBlockStartStack(FILE* File);
  const char* getDefaultTraceName() const OVERRIDE;
  // The map of write cursors associated with table indices.
  typedef std::map<decode::IntType, decode::BitWriteCursor> TableType;
  TableType Table;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_BYTEWRITER_H
