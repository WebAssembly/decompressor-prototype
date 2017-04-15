// -*- C++ -*-
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "Licen// -*- C++ -*-
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

// Defines a writer to convert a CASM stream into the corresponding
// AST algorithm.

#ifndef DECOMPRESSOR_SRC_SEXP_INFLATEAST_H
#define DECOMPRESSOR_SRC_SEXP_INFLATEAST_H

#include "algorithms/casm0x0-lits.h"
#include "utils/ValueStack.h"
#include "interp/Writer.h"

namespace wasm {

namespace filt {

class Algorithm;
class SymbolIndex;
class SymbolTable;

class InflateAst : public interp::Writer {
  InflateAst(const InflateAst&) = delete;
  InflateAst& operator=(const InflateAst&) = delete;

 public:
  explicit InflateAst();
  ~InflateAst() OVERRIDE;
  bool write(decode::IntType Value);
  void setInstallDuringInflation(bool NewValue) {
    InstallDuringInflation = NewValue;
  }
  Algorithm* getGeneratedFile() const;
  void setEnclosingScope(std::shared_ptr<SymbolTable> EnclosingScope) {
    Symtab->setEnclosingScope(EnclosingScope);
  }
  std::shared_ptr<SymbolTable> getSymtab() { return Symtab; }
  decode::StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeTypedValue(decode::IntType Value,
                       interp::IntTypeFormat Format) OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeAction(decode::IntType Action) OVERRIDE;

 private:
  std::shared_ptr<SymbolTable> Symtab;
  std::unique_ptr<SymbolIndex> SymIndex;
  decode::IntType ValuesTop;
  utils::ValueStack<decode::IntType> Values;
  Node* AstsTop;
  utils::ValueStack<Node*> Asts;
  size_t SymbolNameSize;
  size_t ValueMarker;
  size_t AstMarkersTop;
  utils::ValueStack<size_t> AstMarkers;
  bool InstallDuringInflation;

  std::shared_ptr<utils::TraceClass> Trace;

  virtual const char* getDefaultTraceName() const;

  bool applyOp(decode::IntType Op);
  bool appendArgs(Node* Nd);
  template <class T>
  bool buildNullary();
  template <class T>
  bool buildUnary();
  template <class T>
  bool buildBinary();
  template <class T>
  bool buildTernary();
  template <class T>
  bool buildNary();
  bool failBuild(const char* Method, std::string Message);
  bool failWriteActionMalformed();
  bool failWriteHeaderMalformed();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_INFLATEAST_H
