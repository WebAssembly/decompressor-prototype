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

#include "binary/SectionSymbolTable.h"
#include "interp/ReadStream.h"
#include "interp/TraceSexpReader.h"
#include "stream/ByteQueue.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "stream/Cursor.h"
#include "utils/Defs.h"

#include <functional>
#include <vector>

using namespace wasm::interp;

namespace wasm {

namespace filt {

class BinaryReader {
  BinaryReader() = delete;
  BinaryReader(const BinaryReader&) = delete;
  BinaryReader& operator=(const BinaryReader&) = delete;

 public:
  // Returns true if it begins with a WASM file magic number.
  static bool isBinary(const char* Filename);

  BinaryReader(decode::ByteQueue* Input, SymbolTable& Symtab);

  ~BinaryReader() {}

  FileNode* readFile();

  SectionNode* readSection();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

 private:
  alloc::Allocator* Alloc;
  interp::ByteReadStream* Reader;
  decode::Cursor ReadPos;
  SectionSymbolTable SectionSymtab;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  std::vector<Node*> NodeStack;
  TraceClassSexpReader Trace;

  // Reads in a name and returns the read name. Reference is only good till
  // next call to readInternalName() or ReadExternalName().
  InternalName& readInternalName();
  ExternalName& readExternalName();

  void readBlock(std::function<void()> ApplyFn);
  void readSymbolTable();
  void readNode();

  // General ast readers.
  template <class T>
  void readNullary();
  template <class T>
  void readUnary();
  template <class T>
  void readUnarySymbol();
  template <class T>
  void readUnaryUint8();
  template <class T>
  void readUnaryVarint32();
  template <class T>
  void readUnaryVarint64();
  template <class T>
  void readUnaryVaruint32();
  template <class T>
  void readUnaryVaruint64();
  template <class T>
  void readBinary();
  template <class T>
  void readBinarySymbol();
  template <class T>
  void readTernary();
  template <class T>
  void readNary();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
