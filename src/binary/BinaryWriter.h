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
#include "interp/WriteStream.h"
#include "sexp/TraceSexp.h"
#include "stream/Queue.h"
#include "stream/WriteCursor.h"
#include "utils/Defs.h"

#include <functional>

namespace wasm {

namespace filt {

class TextWriter;

class BinaryWriter {
  BinaryWriter() = delete;
  BinaryWriter(const BinaryWriter&) = delete;
  BinaryWriter& operator=(const BinaryWriter&) = delete;

 public:
  BinaryWriter(std::shared_ptr<decode::Queue> Output,
               std::shared_ptr<SymbolTable> Symtab);

  ~BinaryWriter();

  void write(const FileNode* File) { writeNode(File); }

  const decode::WriteCursor& getWritePos() const { return WritePos; }

  void freezeEof() { WritePos.freezeEof(); }

  void setFreezeEofOnDestruct(bool Value) { FreezeEofOnDestruct = Value; }

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

  void setTraceProgress(bool NewValue) {
    getTrace().setTraceProgress(NewValue);
  }

  void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace);
  TraceClassSexp& getTrace();

 private:
  decode::WriteCursorWithTraceContext WritePos;
  std::shared_ptr<interp::WriteStream> Writer;
  SectionSymbolTable SectionSymtab;
  std::shared_ptr<TraceClassSexp> Trace;
  bool MinimizeBlockSize;
  bool FreezeEofOnDestruct;

  void writeNode(const Node* Nd);
  void writeBlock(std::function<void()> ApplyFn);
  void writeSymbol(const Node* Symbol);
  void writePreamble(const FileNode* File);
  void writeLiteral(const Node* Nd);
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_BINGEN_H
