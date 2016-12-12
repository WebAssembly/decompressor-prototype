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

#include "interp/IntStream.h"
#include "interp/Reader.h"
#include "utils/Defs.h"
#include "utils/ValueStack.h"

#include <memory>

namespace wasm {

namespace interp {

class IntReader : public Reader {
  IntReader() = delete;
  IntReader(const IntReader&) = delete;
  IntReader& operator=(const IntReader&) = delete;

 public:
  IntReader(std::shared_ptr<IntStream> Input,
            Writer& Output,
            std::shared_ptr<filt::SymbolTable> Symtab);
  ~IntReader() OVERRIDE;

  bool canFastRead() const OVERRIDE;
  virtual void fastStart() OVERRIDE;
  virtual void fastResume() OVERRIDE;
  virtual void fastReadBackFilled() OVERRIDE;

  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;

 private:
  IntStream::ReadCursorWithTraceContext Pos;
  IntStream::StreamPtr Input;
  // Shows how many are still available since last call to
  // canProcessMoreInputNow().
  size_t StillAvailable;
  // The stack of read cursors (used by peek).
  IntStream::ReadCursor PeekPos;
  utils::ValueStack<IntStream::Cursor> PeekPosStack;

  const char* getDefaultTraceName() const OVERRIDE;
  void describePeekPosStack(FILE* Out) OVERRIDE;

#if 0
  bool fastReadUntil(size_t Eob);
#endif

  decode::IntType read();

  bool canProcessMoreInputNow() OVERRIDE;
  bool stillMoreInputToProcessNow() OVERRIDE;
  bool atInputEob() OVERRIDE;
  void resetPeekPosStack() OVERRIDE;
  void pushPeekPos() OVERRIDE;
  void popPeekPos() OVERRIDE;
  decode::StreamType getStreamType() OVERRIDE;
  bool processedInputCorrectly() OVERRIDE;
  bool enterBlock() OVERRIDE;
  bool exitBlock() OVERRIDE;
  void readFillStart() OVERRIDE;
  void readFillMoreInput() OVERRIDE;
  uint8_t readUint8() OVERRIDE;
  uint32_t readUint32() OVERRIDE;
  uint64_t readUint64() OVERRIDE;
  int32_t readVarint32() OVERRIDE;
  int64_t readVarint64() OVERRIDE;
  uint32_t readVaruint32() OVERRIDE;
  uint64_t readVaruint64() OVERRIDE;
  decode::IntType readValue(const filt::Node* Format) OVERRIDE;

  void fastReadBlock();
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTREADER_H
