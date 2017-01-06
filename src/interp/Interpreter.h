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

#include "interp/StreamReader.h"
#include "interp/StreamWriter.h"
#include "utils/Trace.h"

#include <memory>

namespace wasm {

namespace filt {
class TextWriter;
class FileHeaderNode;
class SymbolTable;
}

namespace interp {

// Defines the algorithm interpreter for filter sections, and the
// corresponding state associated with the interpreter.
//
// TODO(karlschimpf) Rename this to a better name.
class Interpreter FINAL {
  Interpreter() = delete;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

 public:
  Interpreter(std::shared_ptr<decode::Queue> InputStream,
              std::shared_ptr<decode::Queue> OutputStream)
      : Symtab(),
        Input(InputStream, Output, Symtab),
        Output(OutputStream) {}

  ~Interpreter() {}

  void start() { Input.algorithmStart(); }
  void resume() { Input.algorithmResume(); }
  void fail(const std::string& Message) { Input.fail(Message); }
  bool isFinished() const { return Input.isFinished(); }
  bool isSuccessful() const { return Input.isSuccessful(); }
  bool errorsFound() const { return Input.errorsFound(); }

  void addSelector(std::shared_ptr<AlgorithmSelector> Selector) {
    Input.addSelector(Selector);
  }

  // Processes each section in input, and decompresses it (if applicable)
  // to the corresponding output.
  void decompress() {
    start();
    Input.algorithmReadBackFilled();
  }

  void setMinimizeBlockSize(bool NewValue) {
    Output.setMinimizeBlockSize(NewValue);
  }

  void setTraceProgress(bool NewValue);
  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }

 private:
  std::shared_ptr<filt::SymbolTable> Symtab;
  StreamReader Input;
  StreamWriter Output;
  std::shared_ptr<utils::TraceClass> Trace;
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
