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

// Defines a reader from a (non-file based) integer stream.

#ifndef DECOMPRESSOR_SRC_INTERP_INTREADER_H
#define DECOMPRESSOR_SRC_INTERP_INTREADER_H

#include <map>

#include "interp/IntStream.h"
#include "interp/Reader.h"

namespace wasm {

namespace interp {

class IntReader : public Reader {
  IntReader(const IntReader&) = delete;
  IntReader& operator=(const IntReader&) = delete;

 public:
  IntReader(std::shared_ptr<IntStream> Input);
  ~IntReader();

  std::shared_ptr<IntStream> getStream() { return Input; }
  bool hasMoreBlocks() { return Pos.hasMoreBlocks(); }
  IntStream::BlockPtr getNextBlock() { return Pos.getNextBlock(); }
  size_t getIndex() { return Pos.getIndex(); }

  decode::IntType read();
  void describePeekPosStack(FILE* Out) OVERRIDE;

  bool canProcessMoreInputNow() OVERRIDE;
  bool stillMoreInputToProcessNow() OVERRIDE;
  bool atInputEob() OVERRIDE;
  bool atInputEof() OVERRIDE;
  bool pushPeekPos() OVERRIDE;
  bool popPeekPos() OVERRIDE;
  decode::StreamType getStreamType() OVERRIDE;
  bool processedInputCorrectly() OVERRIDE;
  void readFillStart() OVERRIDE;
  void readFillMoreInput() OVERRIDE;
  uint64_t readVaruint64() OVERRIDE;
  bool readBlockEnter() OVERRIDE;
  bool readBlockExit() OVERRIDE;
  bool readHeaderValue(interp::IntTypeFormat Format,
                       decode::IntType& Value) OVERRIDE;
  bool tablePush(decode::IntType Value) OVERRIDE;
  bool tablePop() OVERRIDE;
  utils::TraceContextPtr getTraceContext() OVERRIDE;

 private:
  class TableHandler;

  IntStream::ReadCursor Pos;
  IntStream::StreamPtr Input;
  size_t HeaderIndex;
  // Shows how many are still available since last call to
  // canProcessMoreInputNow().
  size_t StillAvailable;
  IntStream::ReadCursor SavedPos;
  utils::ValueStack<IntStream::Cursor> SavedPosStack;
  TableHandler* TblHandler;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTREADER_H
