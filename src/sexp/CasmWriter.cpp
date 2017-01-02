// -*- C++ -*- */
//
// Copyright 2017 WebAssembly Community Group participants
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

// Implement a class to write a CASM (binary compressed) algorithm file.

#include "sexp/CasmWriter.h"

#include "utils/Defs.h"

#if WASM_BOOT == 0
#include "algorithms/casm0x0.h"
#endif

#include "interp/IntReader.h"
#include "interp/StreamWriter.h"
#include "interp/TeeWriter.h"
#include "sexp/FlattenAst.h"
#include "sexp/InflateAst.h"

namespace wasm {

using namespace filt;
using namespace interp;
using namespace utils;

namespace decode {

CasmWriter::CasmWriter()
    : MinimizeBlockSize(true),
      ErrorsFound(false),
      TraceWriter(false),
      TraceFlatten(false),
      TraceTree(false) {
}

void CasmWriter::writeBinary(std::shared_ptr<SymbolTable> Symtab,
                             std::shared_ptr<IntStream> Output) {
  FlattenAst Flattener(Output, Symtab);
  if (TraceFlatten) {
    auto Trace = std::make_shared<TraceClass>("CastFlattener");
    Trace->setTraceProgress(true);
    Flattener.setTrace(Trace);
  }
  if (!Flattener.flatten())
    ErrorsFound = true;
}

void CasmWriter::writeBinary(std::shared_ptr<SymbolTable> Symtab,
                             std::shared_ptr<Queue> Output,
                             std::shared_ptr<SymbolTable> AlgSymtab) {
  std::shared_ptr<IntStream> IntSeq = std::make_shared<IntStream>();
  writeBinary(Symtab, IntSeq);
  std::shared_ptr<Writer> Writer = std::make_shared<StreamWriter>(Output);
  Writer->setMinimizeBlockSize(MinimizeBlockSize);
  if (TraceTree) {
    auto Tee = std::make_shared<TeeWriter>();
    Tee->add(std::make_shared<InflateAst>(), false, true, false);
    Tee->add(Writer, true, false, true);
    Writer = Tee;
  }
  auto Reader = std::make_shared<IntReader>(IntSeq, *Writer, AlgSymtab);
  if (TraceWriter) {
    auto Trace = std::make_shared<TraceClass>("CasmWriter");
    Trace->setTraceProgress(true);
    Reader->setTrace(Trace);
  }
  Reader->useFileHeader(Symtab->getRootHeader());
  Reader->algorithmStart();
  Reader->algorithmReadBackFilled();
  if (Reader->errorsFound())
    ErrorsFound = true;
}

#if WASM_BOOT == 0
void CasmWriter::writeBinary(std::shared_ptr<filt::SymbolTable> Symtab,
                             std::shared_ptr<Queue> Output) {
  std::shared_ptr<SymbolTable> AlgSymtab;
  install_Algcasm0x0(AlgSymtab);
  writeBinary(Symtab, Output, AlgSymtab);
}
#endif

}  // end of namespace filt

}  // end of namespace wasm
