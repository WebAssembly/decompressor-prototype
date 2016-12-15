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

// Defines the API for all stream writers.

#ifndef DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
#define DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H

#include "sexp/Ast.h"
#include "stream/WriteCursor.h"
#include "utils/Casting.h"

#include <memory>

namespace wasm {

namespace interp {

class State;

class WriteStream : public std::enable_shared_from_this<WriteStream> {
  WriteStream(const WriteStream&) = delete;
  WriteStream& operator=(const WriteStream&) = delete;

 public:
  virtual ~WriteStream();
  void writeUint8(uint8_t Value, decode::WriteCursor& Pos);
  void writeUint32(uint32_t Value, decode::WriteCursor& Pos);
  void writeInt32(int32_t Value, decode::WriteCursor& Pos) {
    writeUint32(uint32_t(Value), Pos);
  }
  void writeUint64(uint64_t Value, decode::WriteCursor& Pos);
  void writeInt64(int64_t Value, decode::WriteCursor& Pos) {
    writeUint64(uint64_t(Value), Pos);
  }
  void writeVarint32(int32_t Value, decode::WriteCursor& Pos);
  void writeVarint64(int64_t Value, decode::WriteCursor& Pos);
  void writeVaruint32(uint32_t Value, decode::WriteCursor& Pos);
  void writeVaruint64(uint64_t Value, decode::WriteCursor& Pos);
  void writeFixedVaruint32(uint32_t Value, decode::WriteCursor& Pos);

  virtual bool writeValue(decode::IntType Value,
                          decode::WriteCursor& Pos,
                          const filt::Node* Format) = 0;

  virtual bool writeAction(decode::WriteCursor& Pos,
                           const filt::CallbackNode* Action) = 0;

  // The following virtuals are used to implement blocks.

  // Returns stream specific address (i.e. bit address for bit streams, byte
  // address for byte streams, and int address for int streams).
  virtual size_t getStreamAddress(decode::WriteCursor& Pos) = 0;

  // Saves the block size using a fixed format that is independent of
  // the block size.
  virtual void writeFixedBlockSize(decode::WriteCursor& Pos,
                                   size_t BlockSize) = 0;
  virtual void writeVarintBlockSize(decode::WriteCursor& Pos,
                                    size_t BlockSIze) = 0;

  // Returns the size of the block, defined by the range of the
  // passed positions (specific to the stream).
  virtual size_t getBlockSize(decode::WriteCursor& StartPos,
                              decode::WriteCursor& EndPos) = 0;

  // Moves Size elemens (stream specific) to StartAddress.
  virtual void moveBlock(decode::WriteCursor& Pos,
                         size_t StartAddress,
                         size_t Size) = 0;

  decode::StreamType getType() const { return Type; }

  decode::StreamType getRtClassId() const { return Type; }
  static bool implementsClass(decode::StreamType /*RtClassId*/) { return true; }

 protected:
  WriteStream(decode::StreamType Type) : Type(Type) {}
  decode::StreamType Type;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_WRITESTREAM_H
