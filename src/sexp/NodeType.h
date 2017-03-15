/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Defines an internal model of filter AST's.
//
// NOTE: template classes define  virtual:
//
//    virtual void forceCompilation();
//
// This is done to force the compilation of virtuals associated with the
// class to be put in file Ast.cpp.
//
// Note: Classes allow an optional allocator as the first argument. This
// allows the creator to decide what allocator will be used internally.

#ifndef DECOMPRESSOR_SRC_SEXP_NODETYPE_H
#define DECOMPRESSOR_SRC_SEXP_NODETYPE_H

#include "utils/Defs.h"
#include "sexp/Ast.def"

namespace wasm {

namespace filt {

enum NodeType : uint32_t {
#define X(tag, opcode, sexp_name, text_num_args, text_max_args) \
  Op##tag = opcode,
  AST_OPCODE_TABLE
#undef X
      NO_SUCH_NODETYPE
};

// Returns the s-expression name
const char* getNodeSexpName(NodeType Type);

// Returns a unique (printable) type name
const char* getNodeTypeName(NodeType Type);

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_NODETYPE_H
