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

#include "casm/InflateAst.h"
#include "interp/IntReader.h"
#include "interp/IntWriter.h"
#include "interp/Interpreter.h"
#include "sexp/Ast.h"
#include "utils/Trace.h"

namespace wasm {

using namespace filt;

namespace interp {

DecompAlgState::DecompAlgState(Interpreter* MyInterpreter)
    : MyInterpreter(MyInterpreter) {}

DecompAlgState::~DecompAlgState() {}

DecompressSelector::DecompressSelector(
    std::shared_ptr<filt::SymbolTable> Symtab,
    std::shared_ptr<DecompAlgState> State)
    : AlgorithmSelector(),
      Symtab(Symtab),
      State(State),
      IsAlgorithm(Symtab->specifiesAlgorithm()) {}

DecompressSelector::~DecompressSelector() {}

std::shared_ptr<SymbolTable> DecompressSelector::getSymtab() {
  return Symtab;
}

bool DecompressSelector::configure(Interpreter* R) {
  return IsAlgorithm ? configureAlgorithm(R) : configureData(R);
}

bool DecompressSelector::configureAlgorithm(Interpreter* R) {
  R->setSymbolTable(Symtab);
  State->OrigWriter = R->getWriter();
  State->Inflator = std::make_shared<InflateAst>();
  State->Inflator->setInstallDuringInflation(false);
  R->setWriter(State->Inflator);
  return true;
}

bool DecompressSelector::applyDataAlgorithm(Interpreter* R) {
  R->setSymbolTable(Symtab);
  return true;
}

bool DecompressSelector::applyNextQueuedAlgorithm(Interpreter* R) {
  SymbolTable::SharedPtr NextSymtab = State->AlgQueue.front();
  R->setSymbolTable(NextSymtab);
  State->AlgQueue.pop();
  if (State->AlgQueue.empty())
    // Define algorithm to convert from integer stream to binary stream.
    State->FinalSymtab =
        State->MyInterpreter->getDefaultAlgorithm(NextSymtab->getWriteHeader());
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
  R->resetSymbolTable();
  return IsAlgorithm ? resetAlgorithm(R) : resetData(R);
}

bool DecompressSelector::resetAlgorithm(Interpreter* R) {
  R->setWriter(State->OrigWriter);
  State->OrigWriter->reset();
  if (!State->Inflator)
    return false;
  Algorithm* Root = State->Inflator->getGeneratedFile();
  std::shared_ptr<SymbolTable> Algorithm = State->Inflator->getSymtab();
  State->Inflator.reset();
  if (Root == nullptr)
    return false;
  constexpr bool UseEnclosing = true;
  Algorithm->setEnclosingScope(State->MyInterpreter->getDefaultAlgorithm(
      Root->getReadHeader(!UseEnclosing)));
  Algorithm->install();
  State->AlgQueue.push(Algorithm);
  return true;
}

bool DecompressSelector::resetData(Interpreter* R) {
  if (!State->IntermediateStream)
    // No decompression applied, just did copy of input. so done!
    return true;
  // Convert intermediate stream back to binary using final symbol table.
  R->setWriter(State->OrigWriter);
  State->OrigWriter.reset();
  auto Input = std::make_shared<IntReader>(State->IntermediateStream);
  State->IntermediateStream.reset();
  R->setInput(Input);
  if (!State->FinalSymtab)
    return false;
  R->setSymbolTable(State->FinalSymtab);
  State->FinalSymtab.reset();
  return true;
}

}  // end of namespace interp

}  // end of namespace wasm
