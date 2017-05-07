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

// Implement functions to read in a CASM (binary compressed) algorithm file.

#include "casm/CasmReader.h"

#include "casm/InflateAst.h"
#include "interp/ByteReader.h"
#include "interp/Interpreter.h"
#include "sexp-parser/Driver.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "stream/FileReader.h"
#include "stream/ReadBackedQueue.h"

namespace wasm {

using namespace filt;
using namespace interp;
using namespace utils;

namespace decode {

CasmReader::CasmReader()
    : Install(true),
      TraceRead(false),
      TraceTree(false),
      TraceLexer(false),
      ErrorsFound(false) {}

CasmReader::~CasmReader() {}

void CasmReader::foundErrors() {
  ErrorsFound = true;
  Symtab.reset();
}

void CasmReader::readText(charstring Filename) {
  std::shared_ptr<SymbolTable> NoEnclosingScope;
  readText(Filename, NoEnclosingScope);
}

void CasmReader::readText(charstring Filename,
                          std::shared_ptr<SymbolTable> EnclosingScope) {
  Symtab = std::make_shared<SymbolTable>(EnclosingScope);
  Driver Parser(Symtab);
  if (TraceRead)
    Parser.setTraceParsing(true);
  if (TraceLexer)
    Parser.setTraceLexing(true);
  if (!Parser.parse(Filename)) {
    foundErrors();
    return;
  }
  if (Install)
    Symtab->install();
  if (TraceTree) {
    TextWriter Writer;
    Writer.write(stderr, Symtab.get());
  }
}

void CasmReader::readBinary(std::shared_ptr<Queue> Binary,
                            std::shared_ptr<SymbolTable> AlgSymtab) {
  auto Inflator = std::make_shared<InflateAst>();
  inflateBinary(Binary, AlgSymtab, Inflator);
}

void CasmReader::readBinary(std::shared_ptr<Queue> Binary,
                            std::shared_ptr<SymbolTable> AlgSymtab,
                            std::shared_ptr<SymbolTable> EnclosingScope) {
  auto Inflator = std::make_shared<InflateAst>();
  Inflator->setEnclosingScope(EnclosingScope);
  inflateBinary(Binary, AlgSymtab, Inflator);
}

void CasmReader::inflateBinary(std::shared_ptr<Queue> Binary,
                               std::shared_ptr<SymbolTable> AlgSymtab,
                               std::shared_ptr<InflateAst> Inflator) {
  InterpreterFlags Flags;
  Interpreter MyReader(std::make_shared<ByteReader>(Binary), Inflator, Flags,
                       AlgSymtab);
  Inflator->setInstallDuringInflation(Install);
  if (TraceRead || TraceTree) {
    auto Trace = std::make_shared<TraceClass>("CasmInterpreter");
    Trace->setTraceProgress(true);
    MyReader.setTrace(Trace);
    if (TraceTree)
      Inflator->setTrace(Trace);
  }
  MyReader.algorithmStart();
  MyReader.algorithmReadBackFilled();
  if (MyReader.errorsFound()) {
    foundErrors();
    return;
  }
  Symtab = Inflator->getSymtab();
  SymbolTable::registerAlgorithm(Symtab);
}

void CasmReader::readBinary(charstring Filename,
                            std::shared_ptr<SymbolTable> AlgSymtab) {
  std::shared_ptr<SymbolTable> EnclosingScope;
  readBinary(Filename, AlgSymtab, EnclosingScope);
}

void CasmReader::readBinary(charstring Filename,
                            std::shared_ptr<SymbolTable> AlgSymtab,
                            std::shared_ptr<SymbolTable> EnclosingScope) {
  readBinary(
      std::make_shared<ReadBackedQueue>(std::make_shared<FileReader>(Filename)),
      AlgSymtab, EnclosingScope);
}

bool CasmReader::hasBinaryHeader(charstring Filename,
                                 std::shared_ptr<SymbolTable> AlgSymtab) {
  return hasBinaryHeader(
      std::make_shared<ReadBackedQueue>(std::make_shared<FileReader>(Filename)),
      AlgSymtab);
}

bool CasmReader::hasBinaryHeader(std::shared_ptr<Queue> Binary,
                                 std::shared_ptr<SymbolTable> AlgSymtab) {
  // Note: This is inefficent, but at least works.
  auto Inflator = std::make_shared<InflateAst>();
  InterpreterFlags Flags;
  Interpreter MyReader(std::make_shared<ByteReader>(Binary), Inflator, Flags,
                       AlgSymtab);
  if (TraceRead || TraceTree) {
    auto Trace = std::make_shared<TraceClass>("CasmInterpreter");
    Trace->setTraceProgress(true);
    MyReader.setTrace(Trace);
    if (TraceTree)
      Inflator->setTrace(Trace);
  }
  MyReader.algorithmStartHasFileHeader();
  MyReader.algorithmReadBackFilled();
  return !MyReader.errorsFound();
}

void CasmReader::readTextOrBinary(charstring Filename,
                                  std::shared_ptr<SymbolTable> EnclosingScope,
                                  std::shared_ptr<SymbolTable> AlgSymtab) {
  if (AlgSymtab) {
    std::shared_ptr<Queue> Binary = std::make_shared<ReadBackedQueue>(
        std::make_shared<FileReader>(Filename));
    // Mark the beginning of the stream, so that it doesn't loose the page.
    ReadCursor Hold(Binary);
    if (hasBinaryHeader(Binary, AlgSymtab))
      return readBinary(Binary, AlgSymtab, EnclosingScope);
  }
  if (std::string(Filename) == "-")
    // Can't reread from stdin, so fail!
    foundErrors();
  else
    readText(Filename, EnclosingScope);
}

}  // end of namespace filt

}  // end of namespace wasm
