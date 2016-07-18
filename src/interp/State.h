/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Models the interpretater for filter s-expressions.

#ifndef DECOMPRESSOR_SRC_INTERP_INTERPSTATE_H
#define DECOMPRESSOR_SRC_INTERP_INTERPSTATE_H

#include "stream/ByteQueue.h"
#include "stream/Cursor.h"
#include "interp/ReadStream.h"
#include "interp/TraceSexpReaderWriter.h"
#include "interp/WriteStream.h"

namespace wasm {

namespace filt {
class TextWriter;
};

namespace interp {

class State {
  State() = delete;
  State(const State&) = delete;
  State& operator=(const State&) = delete;

 public:
  // TODO(kschimpf): Add Output.
  State(decode::ByteQueue* Input,
        decode::ByteQueue* Output,
        filt::SymbolTable* Algorithms);

  ~State() {}

  // Processes each section in input, and decompresses it (if applicable)
  // to the corresponding output.
  void decompress();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

 private:
  decode::ReadCursor ReadPos;
  ReadStream* Reader;
  decode::WriteCursor WritePos;
  WriteStream* Writer;
  alloc::Allocator* Alloc;
  filt::Node* DefaultFormat;
  filt::SymbolTable* Algorithms;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  // The current section name (if applicable).
  std::string CurSectionName;
  bool MinimizeBlockSize = false;
  TraceClassSexpReaderWriter Trace;

  void decompressSection();
  void readSectionName();
  void decompressBlock(const filt::Node* Code);
  // Evaluates code if nonnull. Otherwise copies to end of block.
  void evalOrCopy(const filt::Node* Code);

  // Evaluates Nd. Returns read value if applicable. Zero otherwise.
  decode::IntType eval(const filt::Node* Nd);
  // Reads input as defined by Nd. Returns read value.
  decode::IntType read(const filt::Node* Nd);
  // Writes to output the given value, using format defined by Nd.
  // For convenience, returns written value.
  decode::IntType write(decode::IntType Value, const filt::Node* Nd);
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPSTATE_H
