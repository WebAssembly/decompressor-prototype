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

#include "interp/TraceSexpReader.h"

namespace wasm {

namespace interp {

TraceClassSexpReader::TraceClassSexpReader(decode::Cursor& ReadPos)
    : TraceClassSexp(), ReadPos(ReadPos) {
}

TraceClassSexpReader::TraceClassSexpReader(decode::Cursor& ReadPos,
                                           const char* Label)
    : TraceClassSexp(Label), ReadPos(ReadPos) {
}

TraceClassSexpReader::TraceClassSexpReader(decode::Cursor& ReadPos, FILE* File)
    : TraceClassSexp(File), ReadPos(ReadPos) {
}

TraceClassSexpReader::TraceClassSexpReader(decode::Cursor& ReadPos,
                                           const char* Label,
                                           FILE* File)
    : TraceClassSexp(Label, File), ReadPos(ReadPos) {
}

TraceClassSexpReader::~TraceClassSexpReader() {
}

void TraceClassSexpReader::traceContext() const {
  fprintf(File, "@%" PRIuMAX, uintmax_t(ReadPos.getCurByteAddress()));
  if (!ReadPos.isByteAligned()) {
    fprintf(File, ":%u", ReadPos.getNumExtraBitsRead());
  }
  fputc(' ', File);
}

}  // end of namespace interp

}  // end of namespace wasm
