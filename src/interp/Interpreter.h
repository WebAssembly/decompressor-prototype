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

// Models the interpretater for filter s-expressions.

#ifndef DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
#define DECOMPRESSOR_SRC_INTERP_INTERPRETER_H

#include "interp/Reader.h"
#include "interp/TraceSexpReaderWriter.h"
#include "interp/Writer.h"

namespace wasm {

namespace filt {
class TextWriter;
}

namespace interp {

// Defines the algorithm interpreter for filter sections, and the
// corresponding state associated with the interpreter.
//
// TODO(karlschimpf) Rename this to a better name.
class Interpreter FINAL : public Reader {
  Interpreter() = delete;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

 public:
  Interpreter(std::shared_ptr<decode::Queue> Input,
              std::shared_ptr<decode::Queue> Output,
              std::shared_ptr<filt::SymbolTable> Symtab);

  ~Interpreter() {}

  // Processes each section in input, and decompresses it (if applicable)
  // to the corresponding output.
  void decompress();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  void setMinimizeBlockSize(bool NewValue) {
    Output.setMinimizeBlockSize(NewValue);
  }

  TraceClassSexpReaderWriter& getTrace() { return Trace; }

 private:
  std::shared_ptr<filt::SymbolTable> Symtab;
  Writer Output;
  bool MinimizeBlockSize;
  TraceClassSexpReaderWriter Trace;

  // For debugging only.
  void describeWriteValueStack(FILE* Out);
  void describeBlockStartStack(FILE* Out);
  void describeAllNonemptyStacks(FILE* Out) OVERRIDE;
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
