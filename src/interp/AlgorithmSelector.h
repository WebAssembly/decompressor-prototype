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

// Defines an algorithm selector that provides the API to the interpreter.

#ifndef DECOMPRSSOR_SRC_INTERP_ALGORITHM_SELECTOR
#define DECOMPRSSOR_SRC_INTERP_ALGORITHM_SELECTOR

#include "interp/InterpreterFlags.h"
#include "utils/Defs.h"

namespace wasm {

namespace filt {
class FileHeaderNode;
}  // end of namespace filt

namespace interp {

class Interpreter;

class AlgorithmSelector
    : public std::enable_shared_from_this<AlgorithmSelector> {
  AlgorithmSelector(const AlgorithmSelector&) = delete;
  AlgorithmSelector& operator=(const AlgorithmSelector&) = delete;

 public:
  explicit AlgorithmSelector(const InterpreterFlags& Flags);
  virtual ~AlgorithmSelector();

  // Returns the header to match.
  virtual const filt::FileHeaderNode* getTargetHeader() = 0;

  // Called if header matches. Allows selector to reconfigure the reader.
  // Will read from input if symbol table (i.e. algorith) is set.
  virtual bool configure(Interpreter* R) = 0;

  // Called after reading from file using the symbol table. Allows one
  // to restore/reconfigure the reader.
  virtual bool reset(Interpreter* R) = 0;

  const InterpreterFlags& Flags;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRSSOR_SRC_INTERP_ALGORITHM_SELECTOR
