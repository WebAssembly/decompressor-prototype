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

#include "casm/CasmWriter.h"

#include "interp/ByteWriter.h"
#include "interp/Interpreter.h"
#include "interp/IntReader.h"
#include "interp/IntStream.h"
#include "interp/Writer.h"
#include "interp/TeeWriter.h"
#include "sexp/Ast.h"
#include "casm/FlattenAst.h"
#include "casm/InflateAst.h"

namespace wasm {

using namespace filt;
using namespace interp;
using namespace utils;

namespace decode {

CasmWriter::CasmWriter()
    : MinimizeBlockSize(true),
      FreezeEofAtExit(true),
      ErrorsFound(false),
      BitCompress(false),
      ValidateWhileWriting(false),
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
  if (!Flattener.flatten(BitCompress))
    ErrorsFound = true;
}

const BitWriteCursor& CasmWriter::writeBinary(
    std::shared_ptr<SymbolTable> Symtab,
    std::shared_ptr<Queue> Output,
    std::shared_ptr<SymbolTable> AlgSymtab) {
  std::shared_ptr<IntStream> IntSeq = std::make_shared<IntStream>();
  writeBinary(Symtab, IntSeq);
  auto StrmWriter = std::make_shared<ByteWriter>(Output);
  std::shared_ptr<Writer> Writer = StrmWriter;
  Writer->setMinimizeBlockSize(MinimizeBlockSize);
  if (TraceTree || ValidateWhileWriting) {
    // Inflate as written to verify tree written is correct!
    auto Tee = std::make_shared<TeeWriter>();
    auto Inflator = std::make_shared<InflateAst>();
    Inflator->setEnclosingScope(Symtab->getEnclosingScope());
    Tee->add(Inflator, false, TraceTree, false);
    Tee->add(Writer, true, false, true);
    Writer = Tee;
  }
  InterpreterFlags Flags;
  Interpreter MyReader(std::make_shared<IntReader>(IntSeq), Writer, Flags,
                       AlgSymtab);
  MyReader.setFreezeEofAtExit(FreezeEofAtExit);
  if (TraceWriter || TraceTree) {
    auto Trace = std::make_shared<TraceClass>("CasmWriter");
    Trace->setTraceProgress(true);
    MyReader.setTrace(Trace);
  }
  MyReader.useFileHeader(Symtab->getSourceHeader());
  MyReader.algorithmStart();
  MyReader.algorithmReadBackFilled();
  if (MyReader.errorsFound())
    ErrorsFound = true;
  return StrmWriter->getPos();
}

}  // end of namespace filt

}  // end of namespace wasm
