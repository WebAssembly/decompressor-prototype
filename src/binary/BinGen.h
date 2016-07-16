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

// Defines a binary generator for filter s-expressions.

#ifndef DECOMPRESSOR_SRC_BINARY_BINGEN_H
#define DECOMPRESSOR_SRC_BINARY_BINGEN_H

#include "binary/SectionSymbolTable.h"
#include "stream/ByteQueue.h"
#include "stream/Cursor.h"
#include "interp/TraceSexpWriter.h"
#include "interp/WriteStream.h"
#include "utils/Defs.h"

#include <functional>

namespace wasm {

namespace filt {

class TextWriter;

class BinGen {
  BinGen() = delete;
  BinGen(const BinGen &) = delete;
  BinGen &operator=(const BinGen &) = delete;
public:
  BinGen(decode::ByteQueue *Output, alloc::Allocator *Alloc);

  ~BinGen() {
    WritePos.freezeEob();
  }

  void writePreamble();

  void writeFile(const FileNode *File);
  void writeSection(const SectionNode *Section);

  void setTraceProgress(bool NewValue) {
    Trace.setTraceProgress(NewValue);
  }

  void setMinimizeBlockSize(bool NewValue) {
    MinimizeBlockSize = NewValue;
  }

private:
  decode::WriteCursor WritePos;
  interp::ByteWriteStream *Writer;
  SectionSymbolTable SectionSymtab;
  bool MinimizeBlockSize = false;
  interp::TraceClassSexpWriter Trace;

  void writeNode(const Node *Nd);
  void writeBlock(std::function<void()> ApplyFn);
  void writeSymbol(const Node *Symbol);

#if 0
  // The following are for tracing progress duing binary translation.
  bool TraceProgress = false;
  int IndentLevel = 0;
  TextWriter *TraceWriter = nullptr;
  TextWriter *getTraceWriter();
  void writeIndent();
  void IndentBegin() {
    writeIndent();
    ++IndentLevel;
  }
  void IndentEnd() {
    --IndentLevel;
    writeIndent();
  }
  void enterInternal(const char *Name, bool AddNewline=true);
  void enter(const char *Name, bool AddNewline=true) {
    if (TraceProgress)
      enterInternal(Name, AddNewline);
  }
  void exitInternal(const char *Name);
  void exit(const char *Name) {
    if (TraceProgress)
      exitInternal(Name);
  }
  template<class Type>
  Type returnValueInternal(const char *Name, Type Value);
  template<class Type>
  Type returnValue(const char *Name, Type Value) {
    if (TraceProgress)
      returnValueInternal(Name, Value);
    return Value;
  }
#endif
};

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_BINARY_BINGEN_H
