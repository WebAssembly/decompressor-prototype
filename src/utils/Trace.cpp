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

TraceClass::TraceClass(const char* Lbl) {
  init();
  Label = Lbl;
}

TraceClass::TraceClass(FILE* Fl) {
  init();
  File = Fl;
}

TraceClass::TraceClass(const char* Lbl, FILE* Fl) {
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

void TraceClass::enter(const char* Name) {
  indent();
  ++IndentLevel;
  CallStack.push_back(Name);
  fprintf(File, "enter %s\n", Name);
}

void TraceClass::exit(const char* Name) {
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

void TraceClass::trace_value_label(const char* Label) {
  if (Label == nullptr)
    return;
  fprintf(File, "%s = ", Label);
}

void TraceClass::traceMessageInternal(const std::string& Message) {
  indent();
  fprintf(File, "%s\n", Message.c_str());
}

void TraceClass::traceBoolInternal(const char* Name, bool Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%s\n", Value ? "t" : "f");
}

void TraceClass::traceCharInternal(const char* Name, char Ch) {
  indent();
  trace_value_label(Name);
  fprintf(File, "'%c'\n", Ch);
}

void TraceClass::traceStringInternal(const char* Name, const char* Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "'%s'\n", Value);
}

void TraceClass::traceIntInternal(const char* Name, intmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIiMAX "\n", Value);
}

void TraceClass::traceUintInternal(const char* Name, uintmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIuMAX "\n", Value);
}

void TraceClass::traceIntTypeInternal(const char* Name, IntType Value) {
  indent();
  trace_value_label(Name);
  fprint_IntType(File, Value);
  fputc('\n', File);
}

void TraceClass::traceHexIntTypeInternal(const char* Name, IntType Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIxMAX "\n", uintmax_t(Value));
}

void TraceClass::traceHexInternal(const char* Name, uintmax_t Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%" PRIxMAX "\n", uintmax_t(Value));
}

void TraceClass::tracePointerInternal(const char* Name, void* Value) {
  indent();
  trace_value_label(Name);
  fprintf(File, "%p\n", Value);
}

}  // end of namespace utils

}  // end of namespace wasm
