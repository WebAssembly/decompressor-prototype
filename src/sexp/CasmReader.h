// -*- C++ -*- */
//
// Copyright 2017 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Declare functions to read in a CASM (binary compressed) algorithm file.

// NOTE: In the methods below, if argument Filename == '-', then stdin will
// be used.

#ifndef DECOMPRESSOR_SRC_CASM_CASM_READER_H
#define DECOMPRESSOR_SRC_CASM_CASM_READER_H

#include "sexp/Ast.h"
#include "stream/Queue.h"

#include <memory>

namespace wasm {

namespace decode {

class CasmReader {
  CasmReader(const CasmReader&) = delete;
  CasmReader& operator=(const CasmReader&) = delete;

 public:
  CasmReader();
  ~CasmReader();

  void readText(charstring Filename);
  void readBinary(std::shared_ptr<Queue> Binary,
                  std::shared_ptr<filt::SymbolTable> AlgSymtab);
  void readBinary(charstring Filename,
                  std::shared_ptr<filt::SymbolTable> AlgSymtab);

  bool hasErrors() const { return ErrorsFound; }
  CasmReader& setTraceRead(bool Value) {
    TraceRead = Value;
    return *this;
  }
  CasmReader& setTraceTree(bool Value) {
    TraceTree = Value;
    return *this;
  }
  CasmReader& setTraceLexer(bool Value) {
    TraceLexer = Value;
    return *this;
  }
  std::shared_ptr<filt::SymbolTable> getReadSymtab() { return Symtab; }

 private:
  bool TraceRead;
  bool TraceTree;
  bool TraceLexer;
  bool ErrorsFound;
  std::shared_ptr<filt::SymbolTable> Symtab;
  void foundErrors();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_CASM_CASM_READER_H
