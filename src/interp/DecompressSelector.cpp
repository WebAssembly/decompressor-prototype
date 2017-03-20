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

// Defines algorithm selectors used by the decompressor. That is, it
// assumes that the input is a sequence of algorithms, followed by
// data. The algorithms are read first, and queued. Once the data is
// found, the corresponding data algorithm is added to the
// queue. Then, the reader is configured to run each algorithm (using
// integer streams as the intermediate representation) in the order
// they are queued.  The last algorithm is run using the original
// writer.
//
// To determine which symbol tables are algorithms, if the symbol table
// used to parse the algorithm has the same value for the source and target
// headers. All other algorithms are assumed to a data algorithm that completes
// the decompression.

#include "interp/DecompressSelector.h"

#include "interp/Interpreter.h"
#include "interp/IntReader.h"
#include "interp/IntWriter.h"
#include "sexp/Ast.h"
#include "casm/InflateAst.h"
#include "utils/Trace.h"

namespace wasm {

using namespace filt;

namespace interp {

DecompAlgState::DecompAlgState(Interpreter* MyInterpreter)
    : MyInterpreter(MyInterpreter) {
}

DecompAlgState::~DecompAlgState() {
}

DecompressSelector::DecompressSelector(
    std::shared_ptr<filt::SymbolTable> Symtab,
    std::shared_ptr<DecompAlgState> State)
    : AlgorithmSelector(),
      Symtab(Symtab),
      State(State),
      IsAlgorithm(Symtab->specifiesAlgorithm()) {
}

DecompressSelector::~DecompressSelector() {
}

std::shared_ptr<SymbolTable> DecompressSelector::getSymtab() {
  return Symtab;
}

bool DecompressSelector::configure(Interpreter* R) {
  TRACE_USING(R->getTrace(), bool, "IsAlgorithm", IsAlgorithm);
  return IsAlgorithm ? configureAlgorithm(R) : configureData(R);
}

bool DecompressSelector::configureAlgorithm(Interpreter* R) {
  State->OrigSymtab = R->getSymbolTable();
  R->setSymbolTable(Symtab);
  State->OrigWriter = R->getWriter();
  State->Inflator = std::make_shared<InflateAst>();
  State->Inflator->setInstallDuringInflation(false);
  R->setWriter(State->Inflator);
  return true;
}

bool DecompressSelector::applyDataAlgorithm(Interpreter* R) {
  // Assume this is the last algorithm to apply.
  State->OrigSymtab = R->getSymbolTable();
  R->setSymbolTable(Symtab);
  return true;
}

bool DecompressSelector::applyNextQueuedAlgorithm(Interpreter* R) {
  State->OrigSymtab = R->getSymbolTable();
  R->setSymbolTable(State->AlgQueue.front());
  State->OrigWriter = R->getWriter();
  State->IntermediateStream = std::make_shared<IntStream>();
  R->setWriter(std::make_shared<IntWriter>(State->IntermediateStream));
  return true;
}

bool DecompressSelector::configureData(Interpreter* R) {
  if (State->IntermediateStream &&
      State->MyInterpreter->getFlags().TraceIntermediateStreams)
    State->IntermediateStream->describe(stderr, "Intermediate stream");
  return State->AlgQueue.empty() ? applyDataAlgorithm(R)
                                 : applyNextQueuedAlgorithm(R);
}

bool DecompressSelector::reset(Interpreter* R) {
  return IsAlgorithm ? resetAlgorithm(R) : resetData(R);
}

bool DecompressSelector::resetAlgorithm(Interpreter* R) {
  R->setSymbolTable(State->OrigSymtab);
  State->OrigSymtab.reset();
  R->setWriter(State->OrigWriter);
  State->OrigWriter->reset();
  assert(State->Inflator);
  FileNode* Root = State->Inflator->getGeneratedFile();
  if (Root == nullptr) {
    State->Inflator.reset();
    return false;
  }
  std::shared_ptr<SymbolTable> Algorithm = State->Inflator->getSymtab();
#if 0
  Algorithm->setEnclosingScope(
      State->MyInterpreter->getDefaultAlgorithm(Root->getTargetHeader()));
#else
  Algorithm->setEnclosingScope(
      State->MyInterpreter->getDefaultAlgorithm(Root->getReadHeader()));
#endif
  Algorithm->install(Root);
  State->AlgQueue.push(Algorithm);
  State->Inflator.reset();
  return true;
}

bool DecompressSelector::resetData(Interpreter* R) {
  if (State->AlgQueue.empty()) {
    // TODO(karlschimpf): Why must we null the symbol table?
    std::shared_ptr<SymbolTable> NullSymtab;
    R->setSymbolTable(NullSymtab);
    State->OrigSymtab.reset();
    return true;
  }
  R->setWriter(State->OrigWriter);
  State->OrigWriter.reset();
  if (State->AlgQueue.empty()) {
    // Last run was data algorith to generate final output.
    R->setSymbolTable(State->OrigSymtab);
    State->OrigSymtab.reset();
    // TODO(karlschimpf) Where do we check input for eof?
    return true;
  }
  // Last run was algorithm on front of queue.
  State->AlgQueue.pop();
  assert(State->IntermediateStream);
  auto Input = std::make_shared<IntReader>(State->IntermediateStream);
  R->setInput(Input);
  return configureData(R);
}

}  // end of namespace interp

}  // end of namespace wasm
