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

// Implements the trace utility.

#include "utils/Trace.h"

namespace wasm {

using namespace decode;

namespace utils {

void TraceClass::init() {
  Label = "Trace";
  File = stderr;
  IndentLevel = 0;
  TraceProgress = false;
}

TraceClass::TraceClass() {
  init();
}

TraceClass::TraceClass(charstring Lbl) {
  init();
  Label = Lbl;
}

TraceClass::TraceClass(FILE* Fl) {
  init();
  File = Fl;
}

TraceClass::TraceClass(charstring Lbl, FILE* Fl) {
  init();
  Label = Lbl;
  File = Fl;
}

TraceClass::~TraceClass() {
}

void TraceClass::addContext(ContextPtr NewCtx) {
  if (!NewCtx)
    return;
  for (auto Ctx : ContextList)
    if (NewCtx.get() == Ctx.get())
      return;
  ContextList.push_back(NewCtx);
}

void TraceClass::clearContexts() {
  ContextList.clear();
}

void TraceClass::traceContext() const {
  if (ContextList.empty())
    return;
  for (size_t i = 0; i < ContextList.size(); ++i) {
    if (i > 0)
      fputc('/', File);
    ContextList[i]->describe(File);
  }
  fputc(' ', File);
}

void TraceClass::enter(charstring Name) {
  indent();
  ++IndentLevel;
  CallStack.push_back(Name);
  fprintf(File, "enter %s\n", Name);
}

void TraceClass::exit(charstring Name) {
  assert(~CallStack.empty());
  if (Name == nullptr)
    Name = CallStack.back();
  CallStack.pop_back();
  --IndentLevel;
  indent();
  fprintf(File, "exit %s\n", Name);
}

FILE* TraceClass::indent() {
  for (int i = 0; i < IndentLevel; ++i)
    fputs("  ", File);
  bool AddSeparator = false;
  if (Label) {
    AddSeparator = true;
    fprintf(File, "[%s]", Label);
  }
  if (!CallStack.empty()) {
    AddSeparator = true;
    fputc('.', File);
    fputs(CallStack.back(), File);
  }
  if (AddSeparator)
    fputc(' ', File);
  traceContext();
  return File;
}

FILE* TraceClass::indentNewline() {
  for (int i = 0; i <= IndentLevel; ++i)
    fputs("  ", File);
  return File;
}

void TraceClass::trace_value_label(charstring Label) {
  if (Label == nullptr)
    return;
  fprintf(File, "%s = ", Label);
}

FILE* TraceClass::tracePrefixInternal(const std::string& Message) {
  indent();
  fprintf(File, "%s", Message.c_str());
  return File;
}

void TraceClass::traceMessageInternal(const std::string& Message) {
  indent();
  fprintf(File, "%s\n", Message.c_str());
}

void TraceClass::traceBoolInternal(charstring Name, bool Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%s\n", Value ? "t" : "f");
}

void TraceClass::traceCharInternal(charstring Name, char Ch) {
  indent();
  trace_value_label(Name);
  fprintf(File, "'%c'\n", Ch);
}

void TraceClass::traceStringInternal(charstring Name, charstring Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "'%s'\n", Value);
}

void TraceClass::traceIntInternal(charstring Name, intmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIiMAX "\n", Value);
}

void TraceClass::traceUintInternal(charstring Name, uintmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIuMAX "\n", Value);
}

void TraceClass::traceIntTypeInternal(charstring Name, IntType Value) {
  indent();
  trace_value_label(Name);
  fprint_IntType(File, Value);
  fputc('\n', File);
}

void TraceClass::traceHexIntTypeInternal(charstring Name, IntType Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIxMAX "\n", uintmax_t(Value));
}

void TraceClass::traceHexInternal(charstring Name, uintmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIxMAX "\n", uintmax_t(Value));
}

void TraceClass::tracePointerInternal(charstring Name, void* Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%p\n", Value);
}

}  // end of namespace utils

}  // end of namespace wasm
