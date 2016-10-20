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

// Implements the interpreter for filter s-expressions.

#include "interp/Interpreter.h"

#include "interp/ByteReadStream.h"
#include "interp/ByteWriteStream.h"
#include "sexp/TextWriter.h"
#include "utils/backwards_iterator.h"

#include <iostream>

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

Interpreter::Interpreter(std::shared_ptr<Queue> MyInput,
                         std::shared_ptr<Queue> MyOutput,
                         std::shared_ptr<SymbolTable> Symtab)
    : Reader(MyInput, Output, Symtab, Trace),
      Symtab(Symtab),
      Output(MyOutput, Trace),
      MinimizeBlockSize(false),
      Trace(ReadPos, Output.getPos(), "InterpSexp")
{}

void Interpreter::describeBlockStartStack(FILE* File) {
  Output.describeBlockStartStack(File);
}

void Interpreter::describeAllNonemptyStacks(FILE* File) {
  Reader::describeAllNonemptyStacks(File);
  describeBlockStartStack(File);
}

void Interpreter::decompress() {
  start();
  readBackFilled();
}

}  // end of namespace interp.

}  // end of namespace wasm.
