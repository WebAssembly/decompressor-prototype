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

// Defines how to read filter sections in a WASM file.

#ifndef DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
#define DECOMPRESSOR_SRC_BINARY_BINARYREADER_H

#include "Defs.h"
#include "binary/SectionSymbolTable.h"
#include "stream/ByteQueue.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "stream/Cursor.h"
#include "interp/ReadStream.h"

#include <functional>
#include <vector>

using namespace wasm::interp;

namespace wasm {

namespace filt {

class BinaryReader {
  BinaryReader() = delete;
  BinaryReader(const BinaryReader &) = delete;
  BinaryReader &operator=(const BinaryReader &) = delete;
public:
  BinaryReader(decode::ByteQueue *Input, alloc::Allocator *Alloc);

  ~BinaryReader() {
    delete TraceWriter;
  }

  FileNode *readFile();

  SectionNode *readSection();

  void setTraceProgress(bool NewValue) {
    TraceProgress = NewValue;
  }

private:
  alloc::Allocator *Alloc;
  interp::ByteReadStream *Reader;
  decode::ReadCursor ReadPos;
  SectionSymbolTable SectionSymtab;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  std::vector<Node *> NodeStack;

  // Reads in a name and returns the read name. Reference is only good till
  // next call to readInternalName() or ReadExternalName().
  InternalName &readInternalName();
  ExternalName &readExternalName();

  void readBlock(std::function<void()> ApplyFn);
  void readSymbolTable();
  void readNode();

  // General ast readers.
  template<class T> void readNullary();
  template<class T> void readUnary();
  template<class T> void readUnarySymbol();
  template<class T> void readUnaryUint8();
  template<class T> void readUnaryVarint32();
  template<class T> void readUnaryVarint64();
  template<class T> void readUnaryVaruint32();
  template<class T> void readUnaryVaruint64();
  template<class T> void readBinary();
  template<class T> void readBinarySymbol();
  template<class T> void readTernary();
  template<class T> void readNary();

  // The following are for tracing progress duing binary translation.
  int IndentLevel = 0;
  bool TraceProgress = false;
  TextWriter *TraceWriter = nullptr;
  TextWriter *getTraceWriter();
  void writeReadPos(FILE *File);
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
  void returnValueInternal(const char *Name, Type Value);
  template<class Type>
  Type returnValue(const char *Name, Type Value) {
    if (TraceProgress)
      returnValueInternal(Name, Value);
    return Value;
  }
};

} // end of namespace filt

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
