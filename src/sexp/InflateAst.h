// -*- C++ -*-
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

#include "binary/SectionSymbolTable.h"
#include "interp/Writer.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace filt {

class InflateAst : public interp::Writer {
  InflateAst(const InflateAst&) = delete;
  InflateAst& operator=(const InflateAst&) = delete;

 public:
  explicit InflateAst();
  OVERRIDE ~InflateAst();
  bool write(decode::IntType Value) {
    Values.push(Value);
    return true;
  }
  FileNode* getGeneratedFile() const;
  decode::StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeTypedValue(decode::IntType Value,
                       interp::IntTypeFormat Format) OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

 private:
  std::shared_ptr<SymbolTable> Symtab;
  SectionSymbolTable SectionSymtab;
  size_t ValuesTop;
  utils::ValueStack<decode::IntType> Values;
  Node* Ast;
  utils::ValueStack<Node*> AstStack;
  // TODO(karlschimpe) Can we merge the following two fields
  size_t SymbolNameSize;
  size_t ValueMarker;
  size_t AstMarkerTop;
  utils::ValueStack<size_t> AstMarkerStack;

  std::shared_ptr<TraceClassSexp> Trace;

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
  bool buildNary();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_INFLATEAST_H
