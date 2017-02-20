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

#ifndef DECOMPRESSOR_SRC_INTERP_DECOMPRESSSELECTOR_H_
#define DECOMPRESSOR_SRC_INTERP_DECOMPRESSSELECTOR_H_

#include <queue>

#include "interp/AlgorithmSelector.h"
//#include "utils/Defs.h"
//#include "interp/Interpreter.h"

namespace wasm {

namespace filt {
class FileHeaderNode;
class InflateAst;
class SymbolTable;
}  // end of namespace filt;

namespace interp {

class Interpreter;
class IntStream;
class Writer;

class DecompAlgState : public std::enable_shared_from_this<DecompAlgState> {
  DecompAlgState(const DecompAlgState&) = delete;
  DecompAlgState operator=(const DecompAlgState&) = delete;

 public:
  DecompAlgState();
  virtual ~DecompAlgState();

  // TODO(karlschimpf) Choose a queue data structure instead.
  std::queue<std::shared_ptr<filt::SymbolTable>> AlgQueue;
  std::shared_ptr<filt::InflateAst> Inflator;
  std::shared_ptr<filt::SymbolTable> OrigSymtab;
  std::shared_ptr<Writer> OrigWriter;
  std::shared_ptr<IntStream> IntermediateStream;
};

class DecompressSelector : public AlgorithmSelector {
  DecompressSelector() = delete;
  DecompressSelector(const DecompressSelector&) = delete;
  DecompressSelector& operator=(const DecompressSelector&) = delete;

 public:
  DecompressSelector(std::shared_ptr<filt::SymbolTable> Symtab,
                     std::shared_ptr<DecompAlgState> State,
                     bool IsAlgorithm,
                     const InterpreterFlags& Flags);
  ~DecompressSelector() OVERRIDE;
  const filt::FileHeaderNode* getTargetHeader() OVERRIDE;
  bool configure(Interpreter* R) OVERRIDE;
  bool reset(Interpreter* R) OVERRIDE;

 private:
  std::shared_ptr<filt::SymbolTable> Symtab;
  std::shared_ptr<DecompAlgState> State;
  bool IsAlgorithm;

  bool configureAlgorithm(Interpreter* R);
  bool configureData(Interpreter* R);
  bool resetAlgorithm(Interpreter* R);
  bool resetData(Interpreter* R);
  bool applyDataAlgorithm(Interpreter* R);
  bool applyNextQueuedAlgorithm(Interpreter* R);
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_DECOMPRESSSELECTOR_H_
