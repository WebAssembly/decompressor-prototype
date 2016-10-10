// -*- C++ -*-
//
// Copyright 2016 WebAssembly Community Group participants
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

// Implements adding default decompression rules, once the (booted) source
// is generated.
#include "sexp/Ast.h"

#include "binary/BinaryReader.h"
#include "sexp/defaults.h"
#include "stream/ArrayReader.h"
#include "stream/ReadBackedQueue.h"

namespace wasm {

using namespace decode;

namespace filt {

bool SymbolTable::installPredefinedDefaults(std::shared_ptr<SymbolTable> Symtab,
                                            bool Verbose) {
  auto Stream = std::make_shared<ArrayReader>(getWasmDefaultsBuffer(),
                                              getWasmDefaultsBufferSize());
  BinaryReader Reader(std::make_shared<ReadBackedQueue>(std::move(Stream)),
                      Symtab);
  if (Verbose) {
    Reader.getTrace().setTraceProgress(true);
    TRACE_MESSAGE_USING(Reader.getTrace(),
                        "Loading default decompression rules\n");
  }
  return Reader.readFile() != nullptr;
}

}  // end of namespace filt

}  // end of namespace wasm
