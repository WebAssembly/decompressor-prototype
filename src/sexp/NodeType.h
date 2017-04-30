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

#include "sexp/Ast-defs.h"
#include "utils/Defs.h"

namespace wasm {

namespace filt {

enum class NodeType : uint32_t {
#define X(tag, opcode, sexp_name, text_num_args, text_max_args, NSL, hidden) \
  tag = opcode,
  AST_OPCODE_TABLE
#undef X
      NO_SUCH_NODETYPE = std::numeric_limits<uint32_t>::max()
};

static constexpr size_t NumNodeTypes = 0
#define X(tag, opcode, sexp_name, text_num_args, text_max_args, NSL, hidden) +1
    AST_OPCODE_TABLE
#undef X
    ;

static constexpr size_t MaxNodeType = const_maximum(
#define X(tag, opcode, sexp_name, text_num_args, text_max_args, NSL, hidden) \
  size_t(opcode),
    AST_OPCODE_TABLE
#undef X
        std::numeric_limits<size_t>::min());

struct AstTraitsType {
  NodeType Type;
  const char* TypeName;
  const char* SexpName;
  int NumTextArgs;
  int AdditionalTextArgs;
  bool NeverSameLineInText;
  bool HidesSeqInText;
};

extern const AstTraitsType* getAstTraits(NodeType Type);

// Returns the s-expression name
const char* getNodeSexpName(NodeType Type);

// Returns a unique (printable) type name
const char* getNodeTypeName(NodeType Type);

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_NODETYPE_H
