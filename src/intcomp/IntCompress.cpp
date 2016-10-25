// -*- C++ -*- */
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

// Implements the compressor of WASM fiels base on integer usage.

#include "intcomp/IntCompress.h"

namespace wasm {

using namespace interp;

namespace intcomp {

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                             std::shared_ptr<decode::Queue> OutputStream,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Symtab(Symtab),
      Input(InputStream, Output, Symtab, Trace),
      Output(OutputStream, Trace),
      Trace(Input.getPos(), Output.getPos(), "IntCompress") {
}

void IntCompressor::compress() {
  Input.start();
  Input.readBackFilled();
}

} // end of namespace intcomp

} // end of namespace wasm
