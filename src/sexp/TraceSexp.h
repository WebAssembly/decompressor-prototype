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

// Extends the trace class to handle filter s-expressions.

#ifndef DECOMPRESSOR_SRC_SEXP_TRACESEXP_H
#define DECOMPRESSOR_SRC_SEXP_TRACESEXP_H

#include "utils/Trace.h"

namespace wasm {

namespace filt {

class Node;
class TextWriter;

class TraceClassSexp : public utils::TraceClass {
  TraceClassSexp(const TraceClassSexp&) = delete;
  TraceClassSexp& operator=(const TraceClassSexp&) = delete;

 public:
  TraceClassSexp();
  TraceClassSexp(const char* Label);
  TraceClassSexp(FILE* File);
  TraceClassSexp(const char* Label, FILE* File);
  ~TraceClassSexp();
  void traceSexp(const Node* Nd) {
    if (TraceProgress)
      traceSexpInternal("", Nd);
  }

  void traceSexp(const char *Prefix, const Node* Nd) {
    if (TraceProgress)
      traceSexpInternal(Prefix, Nd);
  }

  void errorSexp(const char *Prefix, const Node* Nd);

  void errorSexp(const Node* Nd) {
    errorSexp("", Nd);
  }

 protected:
  TextWriter* Writer;
  void traceSexpInternal(const char *Prefix, const Node* Nd);

  TextWriter* getTextWriter() {
    if (Writer == nullptr)
      Writer = getNewTextWriter();
    return Writer;
  }

  TextWriter* getNewTextWriter();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_TRACEEXP_H
