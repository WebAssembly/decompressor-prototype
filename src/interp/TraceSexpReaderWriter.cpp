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

#include "interp/TraceSexpReaderWriter.h"

namespace wasm {

namespace interp {

TraceClassSexpReaderWriter::TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                                                       decode::Cursor& WritePos)
    : TraceClassSexp(), ReadPos(ReadPos), WritePos(WritePos) {
}

TraceClassSexpReaderWriter::TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                                                       decode::Cursor& WritePos,
                                                       const char* Label)
    : TraceClassSexp(Label), ReadPos(ReadPos), WritePos(WritePos) {
}

TraceClassSexpReaderWriter::TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                                                       decode::Cursor& WritePos,
                                                       FILE* File)
    : TraceClassSexp(File), ReadPos(ReadPos), WritePos(WritePos) {
}

TraceClassSexpReaderWriter::TraceClassSexpReaderWriter(decode::Cursor& ReadPos,
                                                       decode::Cursor& WritePos,
                                                       const char* Label,
                                                       FILE* File)
    : TraceClassSexp(Label, File), ReadPos(ReadPos), WritePos(WritePos) {
}

TraceClassSexpReaderWriter::~TraceClassSexpReaderWriter() {
}

void TraceClassSexpReaderWriter::traceContext() const {
  ReadPos.describe(File);
  fputc('/', File);
  WritePos.describe(File);
  fputc(' ', File);
}

}  // end of namespace interp

}  // end of namespace wasm
