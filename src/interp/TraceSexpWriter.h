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

#ifndef DECOMPRESSOR_SRC_INTERP_TRACESEXPWRITER_H
#define DECOMPRESSOR_SRC_INTERP_TRACESEXPWRITER_H

#include "sexp/TraceSexp.h"
#include "stream/Cursor.h"

namespace wasm {

namespace interp {

// Extends the sexpression trace class to add a read cursor context.
class TraceClassSexpWriter : public filt::TraceClassSexp {
  TraceClassSexpWriter(const TraceClassSexpWriter&) = delete;
  TraceClassSexpWriter& operator=(const TraceClassSexpWriter&) = delete;

 public:
  explicit TraceClassSexpWriter(decode::Cursor& WritePos);
  TraceClassSexpWriter(decode::Cursor& WritePos, const char* Label);
  TraceClassSexpWriter(decode::Cursor& WritePos, FILE* File);
  TraceClassSexpWriter(decode::Cursor& WritePos, const char* Label, FILE* File);
  ~TraceClassSexpWriter();

  void traceContext() const OVERRIDE;

 protected:
  decode::Cursor& WritePos;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_TRACESEXPWRITER_H
