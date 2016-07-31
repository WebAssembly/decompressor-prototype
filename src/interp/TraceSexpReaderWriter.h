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

#ifndef DECOMPRESSOR_SRC_INTERP_TRACESEXPREADERWRITER_H
#define DECOMPRESSOR_SRC_INTERP_TRACESEXPREADERWRITER_H

#include "sexp/TraceSexp.h"
#include "stream/Cursor.h"

namespace wasm {

namespace interp {

// Extends the sexpression trace class to add a read cursor context.
class TraceClassSexpReaderWriter : public filt::TraceClassSexp {
  TraceClassSexpReaderWriter(const TraceClassSexpReaderWriter&) = delete;
  TraceClassSexpReaderWriter& operator=(const TraceClassSexpReaderWriter&) =
      delete;

 public:
  explicit TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                                      decode::Cursor& WritePos);
  TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                             decode::Cursor& WritePos,
                             const char* Label);
  TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                             decode::Cursor& WritePos,
                             FILE* File);
  TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                             decode::Cursor& WritePos,
                             const char* Label,
                             FILE* File);
  ~TraceClassSexpReaderWriter();

  void traceContext() const OVERRIDE;

  bool getTraceIoDifference() const { return TraceIoDifference; }

  void setTraceIoDifference(bool NewValue) { TraceIoDifference = NewValue; }

 protected:
  decode::Cursor& ReadPos;
  decode::Cursor& WritePos;
  bool TraceIoDifference;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_TRACESEXPREADERWRITER_H
