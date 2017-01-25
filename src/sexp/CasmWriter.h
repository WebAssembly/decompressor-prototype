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

// Declare a class to write a CASM (binary compressed) algorithm file.

#ifndef DECOMPRESSOR_SRC_CASM_CASM_WRITER_H
#define DECOMPRESSOR_SRC_CASM_CASM_WRITER_H

#include "interp/IntStream.h"
#include "utils/Defs.h"
#include "sexp/Ast.h"
#include "stream/Queue.h"
#if 0
#include "stream/WriteCursor.h"
#else
#include "stream/BitWriteCursor.h"
#endif
#include "utils/Defs.h"

#include <memory>

namespace wasm {

namespace decode {

class CasmWriter {
  CasmWriter(const CasmWriter&) = delete;
  CasmWriter& operator=(const CasmWriter&) = delete;

 public:
  CasmWriter();

  // Write algorithm in Symtab to integer stream
  void writeBinary(std::shared_ptr<filt::SymbolTable> Symtab,
                   std::shared_ptr<interp::IntStream> Output);

  // Write aglorithm in Symtab to Output, using CASM algorithm in AlgSymtab.
  // Returns final write position.
#if 0
  const decode::WriteCursor& writeBinary(
      std::shared_ptr<filt::SymbolTable> Symtab,
      std::shared_ptr<Queue> Output,
      std::shared_ptr<filt::SymbolTable> AlgSymtab);
#else
  const BitWriteCursor& writeBinary(
      std::shared_ptr<filt::SymbolTable> Symtab,
      std::shared_ptr<Queue> Output,
      std::shared_ptr<filt::SymbolTable> AlgSymtab);
#endif

#if WASM_BOOT == 0
  // Same as above, but using default aglorithm casm0x0.
#if 0
  const decode::WriteCursor& writeBinary(
      std::shared_ptr<filt::SymbolTable> Symtab,
      std::shared_ptr<Queue> Output);
#else
  const BitWriteCursor& writeBinary(
      std::shared_ptr<filt::SymbolTable> Symtab,
      std::shared_ptr<Queue> Output);
#endif
#endif

  bool hasErrors() const { return ErrorsFound; }

  CasmWriter& setMinimizeBlockSize(bool Value) {
    MinimizeBlockSize = Value;
    return *this;
  }

  CasmWriter& setFreezeEofAtExit(bool Value) {
    FreezeEofAtExit = Value;
    return *this;
  }

  CasmWriter& setTraceWriter(bool Value) {
    TraceWriter = Value;
    return *this;
  }

  CasmWriter& setTraceFlatten(bool Value) {
    TraceFlatten = Value;
    return *this;
  }

  CasmWriter& setTraceTree(bool Value) {
    TraceTree = Value;
    return *this;
  }

 private:
  bool MinimizeBlockSize;
  bool FreezeEofAtExit;
  bool ErrorsFound;
  bool TraceWriter;
  bool TraceFlatten;
  bool TraceTree;
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_CASM_CASM_WRITER_H
