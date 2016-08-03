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

#include "interp/TraceSexpWriter.h"

namespace wasm {

namespace interp {

TraceClassSexpWriter::TraceClassSexpWriter(decode::Cursor& WritePos)
    : TraceClassSexp(), WritePos(WritePos) {
}

TraceClassSexpWriter::TraceClassSexpWriter(decode::Cursor& WritePos,
                                           const char* Label)
    : TraceClassSexp(Label), WritePos(WritePos) {
}

TraceClassSexpWriter::TraceClassSexpWriter(decode::Cursor& WritePos, FILE* File)
    : TraceClassSexp(File), WritePos(WritePos) {
}

TraceClassSexpWriter::TraceClassSexpWriter(decode::Cursor& WritePos,
                                           const char* Label,
                                           FILE* File)
    : TraceClassSexp(Label, File), WritePos(WritePos) {
}

TraceClassSexpWriter::~TraceClassSexpWriter() {
}

void TraceClassSexpWriter::traceContext() const {
  fprintf(File, "@%" PRIuMAX, uintmax_t(WritePos.getCurByteAddress()));
  if (!WritePos.isByteAligned())
    fprintf(File, ":%" PRIuMAX, uintmax_t(WritePos.getNumExtraBitsWritten()));
  fputc(' ', File);
}

}  // end of namespace interp

}  // end of namespace wasm
