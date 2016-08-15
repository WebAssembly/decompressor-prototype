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

#ifdef NDEBUG
#define TRACE_SEXP(Name, Nd)
#define TRACE_SEXP_USING(Trace, Name, Nd)
#else
#define TRACE_SEXP(Name, Nd) TRACE(node_ptr, (Name), (Nd))
#define TRACE_SEXP_USING(trace, Name, Nd)                                      \
  TRACE_USING((Trace), node_ptr, (Name), (Nd))
#endif

#include "sexp/NodeType.h"
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

  void trace_node_ptr(const char* Name, const Node* Nd) {
    traceSexpInternal(Name, Nd);
  }
  void trace_NodeType(const char* Name, NodeType Type) {
    traceNodeTypeInternal(Name, Type);
  }

  void printSexp(const char* Name, const Node* Nd);

  void printSexp(const Node* Nd) { printSexp(nullptr, Nd); }

  TextWriter* getTextWriter() {
    if (Writer == nullptr)
      Writer = getNewTextWriter();
    return Writer;
  }

  // TODO(karlschimpf): Methods below here are deprecated.
  void traceSexp(const Node* Nd) {
    if (getTraceProgress())
      traceSexpInternal("", Nd);
  }

  void traceSexp(const char* Name, const Node* Nd) {
    if (getTraceProgress())
      traceSexpInternal(Name, Nd);
  }

  void traceNodeType(NodeType Type) {
    if (getTraceProgress())
      traceNodeTypeInternal("", Type);
  }

  void traceNodeType(const char* Name, NodeType Type) {
    if (getTraceProgress())
      traceNodeTypeInternal(Name, Type);
  }

  void errorSexp(const char* Name, const Node* Nd) {
    printSexp(Name, Nd);
  }

  void errorSexp(const Node* Nd) { printSexp(nullptr, Nd); }

 protected:
  TextWriter* Writer;
  void traceSexpInternal(const char* Name, const Node* Nd);
  void traceNodeTypeInternal(const char* Name, NodeType Type);

  TextWriter* getNewTextWriter();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_TRACEEXP_H
