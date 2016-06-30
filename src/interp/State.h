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

#include "stream/Cursor.h"
#include "interp/ReadStream.h"
#include "interp/WriteStream.h"

namespace wasm {

namespace interp {

class State {
  State() = delete;
  State(const State &) = delete;
  State &operator=(const State &) = delete;
public:
  // TODO(kschimpf): Add Output.
  State(decode::ByteQueue *Input, decode::ByteQueue *Output);
  decode::ReadCursor ReadPos;
  ReadStream *Reader;
  decode::WriteCursor WritePos;
  WriteStream *Writer;
  alloc::Allocator *Alloc;
  wasm::filt::Node *DefaultFormat;
  // Evaluates Nd. Returns read value if applicable. Zero otherwise.
  decode::IntType eval(const wasm::filt::Node *Nd);
  decode::IntType read(const wasm::filt::Node *Nd);
  decode::IntType write(decode::IntType Value, const wasm::filt::Node *Nd);
};

} // end of namespace interp.

} // end of namespace wasm.

#endif // DECOMPRESSOR_SRC_INTERP_INTERPSTATE_H
