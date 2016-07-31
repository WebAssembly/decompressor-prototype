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

namespace utils {

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

void TraceClass::traceContext() const {
}

void TraceClass::enter(const char* Name) {
  indent();
  ++IndentLevel;
  CallStack.push_back(Name);
  fprintf(File, "enter %s\n", Name);
}

void TraceClass::exit() {
  assert(~CallStack.empty());
  const char* Name = CallStack.back();
  CallStack.pop_back();
  --IndentLevel;
  indent();
  fprintf(File, "exit %s\n", Name);
}

void TraceClass::indent() {
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
}

void TraceClass::traceMessageInternal(const std::string& Message) {
  indent();
  fprintf(File, "%s\n", Message.c_str());
}

void TraceClass::traceBoolInternal(const char* Name, bool Value) {
  indent();
  fprintf(File, "%s = %s\n", Name, Value ? "t" : "f");
}

void TraceClass::traceCharInternal(const char* Name, char Ch) {
  indent();
  fprintf(File, "%s = '%c'\n", Name, Ch);
}

void TraceClass::traceStringInternal(const char* Name, std::string& Value) {
  indent();
  fprintf(File, "%s = '%s'\n", Name, Value.c_str());
}

void TraceClass::traceIntInternal(const char* Name, intmax_t Value) {
  indent();
  fprintf(File, "%s = %" PRIiMAX "\n", Name, Value);
}

void TraceClass::traceUintInternal(const char* Name, uintmax_t Value) {
  indent();
  fprintf(File, "%s = %" PRIuMAX "\n", Name, Value);
}

void TraceClass::traceHexInternal(const char* Name, uintmax_t Value) {
  indent();
  fprintf(File, "%s = %" PRIxMAX "\n", Name, uintmax_t(Value));
}

void TraceClass::tracePointerInternal(const char* Name, void* Value) {
  indent();
  fprintf(File, "%s = %p\n", Name, Value);
}

}  // end of namespace utils

}  // end of namespace wasm
