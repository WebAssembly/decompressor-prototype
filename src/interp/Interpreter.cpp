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

#include <cassert>

namespace wasm {

using namespace filt;
using namespace utils;

namespace interp {

SymbolTableSelector::SymbolTableSelector(std::shared_ptr<SymbolTable> Symtab,
                                         bool DataSelector)
    : AlgorithmSelector(Symtab->getTargetHeader(), DataSelector),
      Symtab(Symtab),
      StillGood(true)
{}

SymbolTableSelector::~SymbolTableSelector() {}

std::shared_ptr<SymbolTable> SymbolTableSelector::getAlgorithm() {
  assert(StillGood && "SymbolTableSelector getAlgorithm called more than once!");
  StillGood = false;
  return Symtab;
}

void Interpreter::addSelector(std::shared_ptr<AlgorithmSelector> Selector) {
  Selectors.push_back(Selector);
}

void Interpreter::start() {
  assert(!Selectors.empty());
  // TODO(karlschimpf) Find correct symbol table to use.
  Input.setSymbolTable(Selectors.front()->getAlgorithm());
  Input.algorithmStart();
}

void Interpreter::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  return getTrace().setTraceProgress(NewValue);
}

void Interpreter::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (Trace) {
    Input.setTrace(Trace);
    Output.setTrace(Trace);
  }
}

std::shared_ptr<TraceClass> Interpreter::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("InterpSexp"));
  return Trace;
}

}  // end of namespace interp.

}  // end of namespace wasm.
