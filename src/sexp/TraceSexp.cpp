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

// Implements trace class to handle filter s-expressions.

#include "sexp/TraceSexp.h"
#include "sexp/TextWriter.h"

namespace wasm {

namespace filt {

TraceClassSexp::TraceClassSexp() : TraceClass(), Writer(nullptr) {
}

TraceClassSexp::TraceClassSexp(const char* Label)
    : TraceClass(Label), Writer(nullptr) {
}

TraceClassSexp::TraceClassSexp(FILE* File) : TraceClass(File), Writer(nullptr) {
}

TraceClassSexp::TraceClassSexp(const char* Label, FILE* File)
    : TraceClass(Label, File), Writer(nullptr) {
}

TraceClassSexp::~TraceClassSexp() {
  delete Writer;
}

TextWriter* TraceClassSexp::getNewTextWriter() {
  return new TextWriter();
}

void TraceClassSexp::traceSexpInternal(const char *Prefix, const Node* Node) {
  indent();
  fprintf(File, "%s", Prefix);
  getTextWriter()->writeAbbrev(File, Node);
}

void TraceClassSexp::errorSexp(const char *Prefix, const Node* Node) {
  fprintf(File, "%s", Prefix);
  getTextWriter()->write(File, Node);
}

}  // end of namespace filt

}  // end of namespace wasm
