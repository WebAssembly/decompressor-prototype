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

// Defines the interpretater for filter s-expressions.

#include "interp/Interpreter.h"

namespace wasm {

using namespace filt;

namespace interp {

void Interpreter::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (Trace) {
    Input.setTrace(Trace);
    Output.setTrace(Trace);
  }
}

TraceClassSexp& Interpreter::getTrace() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("InterpSexp"));
  return *Trace;
}

}  // end of namespace interp.

}  // end of namespace wasm.
