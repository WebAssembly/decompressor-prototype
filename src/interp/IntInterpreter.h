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

// Defines a reader from a (non-file based) integer stream.

#ifndef DECOMPRESSOR_SRC_INTERP_INTINTERPRETER_H_
#define DECOMPRESSOR_SRC_INTERP_INTINTERPRETER_H_

#include "interp/Interpreter.h"

namespace wasm {

namespace interp {

class IntReader;

class IntInterpreter : public Interpreter {
  IntInterpreter() = delete;
  IntInterpreter(const IntInterpreter&) = delete;
  IntInterpreter& operator=(const IntInterpreter&) = delete;

 public:
  IntInterpreter(std::shared_ptr<IntReader> Input,
                 std::shared_ptr<Writer> Output,
                 const InterpreterFlags& Flags,
                 std::shared_ptr<filt::SymbolTable> Symtab);
  ~IntInterpreter() OVERRIDE;

  // Read based on structure of input integer stream.
  void structuralStart();
  void structuralRead();
  void structuralReadBackFilled();
  void structuralResume();

 private:
  const char* getDefaultTraceName() const OVERRIDE;
  std::shared_ptr<IntReader> IntInput;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTINTERPRETER_H_
