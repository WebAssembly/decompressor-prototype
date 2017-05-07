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

// Defines Opcodes for Ast nodes.

#ifndef DECOMPRESSOR_SRC_SEXP_AST_DEFS_H_
#define DECOMPRESSOR_SRC_SEXP_AST_DEFS_H_

// Defines common declarations for nodes.

#define VALIDATENODE \
 public:             \
  bool validateNode(ConstNodeVectorType& Parents) const OVERRIDE;

#define GETINTNODE \
 public:           \
  const IntegerNode* getIntNode() const;

#define GETDEF(NAME) \
 public:             \
  const NAME##Def* getDef() const;

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_NULLARYNODE_TABLE \
  X(Bit, Nullary, , )         \
  X(Error, Nullary, , )       \
  X(LastRead, Nullary, , )    \
  X(NoLocals, Nullary, , )    \
  X(NoParams, Nullary, , )    \
  X(Uint32, Nullary, , )      \
  X(Uint64, Nullary, , )      \
  X(Uint8, Nullary, , )       \
  X(Varint32, Nullary, , )    \
  X(Varint64, Nullary, , )    \
  X(Varuint32, Nullary, , )   \
  X(Varuint64, Nullary, , )   \
  X(Void, Nullary, , )

//#define X(NAME, FORMAT, DEFAULT, MERGE, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   FORMAT: The binary format (in binary files) of the value.
//   DEFAULT: Default value assumed if not provided.
//   MERGE: True if instances can be merged.
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_INTEGERNODE_TABLE                                \
  X(I32Const, Varint32, 0, true, IntegerNode, , )            \
  X(I64Const, Varint64, 0, true, IntegerNode, , )            \
  X(Local, Varuint32, 0, true, IntegerNode, VALIDATENODE, )  \
  X(Locals, Varuint32, 0, true, IntegerNode, , )             \
  X(Param, Varuint32, 0, false, IntegerNode, VALIDATENODE, ) \
  X(ParamCached, Varuint32, 1, false, IntegerNode, , )       \
  X(ParamExprs, Varuint32, 0, false, IntegerNode, , )        \
  X(ParamExprsCached, Varuint32, 0, false, IntegerNode, , )  \
  X(ParamValues, Varuint32, 0, false, IntegerNode, , )       \
  X(U8Const, Uint8, 0, true, IntegerNode, , )                \
  X(U32Const, Varuint32, 0, true, IntegerNode, , )           \
  X(U64Const, Varuint64, 0, true, IntegerNode, , )

// Defines explicit literals
//#define X(NAME, BASE, VALUE, FORMAT, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   VALUE: The value of the literal.
//   FORMAT: The ValueFormat to use to represent the literal.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_LITERAL_TABLE              \
  X(Zero, IntegerNode, 0, Decimal, , ) \
  X(One, IntegerNode, 1, Decimal, , )

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_UNARYNODE_TABLE                                                   \
  X(AlgorithmFlag, Unary, , )                                                 \
  X(AlgorithmName, Unary, , )                                                 \
  X(Block, Unary, , )                                                         \
  X(BitwiseNegate, Unary, , )                                                 \
  X(Callback, Unary, VALIDATENODE GETINTNODE, )                               \
  X(LastSymbolIs, Unary, , )                                                  \
  X(LiteralActionUse, Unary, VALIDATENODE GETINTNODE GETDEF(LiteralAction), ) \
  X(LiteralUse, Unary, VALIDATENODE GETINTNODE GETDEF(Literal), )             \
  X(LoopUnbounded, Unary, , )                                                 \
  X(Not, Unary, , )                                                           \
  X(Peek, Unary, , )                                                          \
  X(Read, Unary, , )                                                          \
  X(Undefine, Unary, , )                                                      \
  X(UnknownSection, Unary, , )

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_BINARYNODE_TABLE      \
  X(And, Binary, , )              \
  X(BinarySelect, Binary, , )     \
  X(BitwiseAnd, Binary, , )       \
  X(BitwiseOr, Binary, , )        \
  X(BitwiseXor, Binary, , )       \
  X(Case, Binary, CASE_DECLS, )   \
  X(IfThen, Binary, , )           \
  X(LiteralActionDef, Binary, , ) \
  X(LiteralDef, Binary, , )       \
  X(Loop, Binary, , )             \
  X(Or, Binary, , )               \
  X(Rename, Binary, , )           \
  X(Set, Binary, , )              \
  X(Table, Binary, , )

#define CASE_DECLS                                     \
  VALIDATENODE                                         \
 public:                                               \
  decode::IntType getValue() const { return Value; }   \
  const Node* getCaseBody() const { return CaseBody; } \
                                                       \
 private:                                              \
  mutable decode::IntType Value;                       \
  mutable const Node* CaseBody;

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_SELECTNODE_TABLE \
  X(Switch, SelectBase, , )  \
  X(Map, SelectBase, , )

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_NARYNODE_TABLE                     \
  X(Algorithm, Nary, ALGORITHM_DECLS, init();) \
  X(Define, Nary, DEFINE_DECLS, )              \
  X(EnclosingAlgorithms, Nary, , )             \
  X(EvalVirtual, Eval, , )                     \
  X(LiteralActionBase, Nary, , )               \
  X(ParamArgs, Nary, , )                       \
  X(ReadHeader, Header, , )                    \
  X(Sequence, Nary, , )                        \
  X(SourceHeader, Header, , )                  \
  X(Write, Nary, , )                           \
  X(WriteHeader, Header, , )

//#define X(NAME, BASE, DECLS, INIT)
// where:
//   NAME: Class/enum node type name
//   BASE: Base class for node class.
//   DECLS: Other declarations for the node.
//   INIT: code to run in the body of the constructors to finish
//         initialization
#define AST_TERNARYNODE_TABLE X(IfThenElse, Ternary, , )

//#define X(NAME)
// where:
//   NAME: Class/enum node type name
#define AST_CACHEDNODE_TABLE \
  X(SymbolDefn)              \
  X(IntLookup)

// The rest of this file defines text printing rules.

//#define X(NAME)
#define AST_TEXTINVISIBLE_TABLE \
  X(NoLocals)                   \
  X(NoParams)

//#define X(tag, opcode, sexp_name, text_num_args, text_max_args, NSL, hidden)
// where:
//   tag: enumeration name.
//   opcode: opcode value (also enum value).
//   sexp_name: print name in s-expressions.
//   text_num_args: Minimum number of arguments to print on same line in
//                  TextWriter.
//   text_max_args: Number of additional arguments (above text_num_args)
//                  that can appear on same line.
//   NSL: true if arguments should never appear on the same line as the
//        operator in TextWriter.
//   hidden: true if a sequence operator appearing as operator should be hidden
//           in text writer.
#define AST_OPCODE_TABLE                                                 \
  /* enum name, opcode */                                                \
                                                                         \
  /* Control flow operators */                                           \
  X(Block, 0x01, "block", 1, 0, true, true)                              \
  X(Case, 0x02, "case", 2, 0, true, true)                                \
  X(Error, 0x03, "error", 0, 0, false, false)                            \
  X(EvalVirtual, 0x04, "eval", 1, 1, false, false)                       \
  X(Loop, 0x07, "loop", 1, 1, true, true)                                \
  X(LoopUnbounded, 0x08, "loop.unbounded", 0, 1, true, true)             \
  X(Switch, 0x09, "switch", 1, 0, true, false)                           \
  X(Sequence, 0x0a, "seq", 0, 0, true, false)                            \
  X(IfThen, 0x0b, "if", 1, 0, true, false)                               \
  X(IfThenElse, 0x0c, "if", 1, 0, true, false)                           \
                                                                         \
  /* Constants */                                                        \
  X(Void, 0x10, "void", 0, 0, false, false)                              \
  X(Symbol, 0x11, "symbol", 0, 0, false, false)                          \
  X(I32Const, 0x12, "i32.const", 1, 0, false, false)                     \
  X(I64Const, 0x13, "i64.const", 1, 0, false, false)                     \
  X(U8Const, 0x14, "u8.const", 1, 0, false, false)                       \
  X(U32Const, 0x15, "u32.const", 1, 0, false, false)                     \
  X(U64Const, 0x16, "u64.const", 1, 0, false, false)                     \
  X(Zero, 0x17, "0", 0, 0, false, false)                                 \
  X(One, 0x18, "1", 0, 0, false, false)                                  \
                                                                         \
  /* Formatting */                                                       \
  X(Uint32, 0x20, "uint32", 0, 0, false, false)                          \
  X(Uint64, 0x21, "uint64", 0, 0, false, false)                          \
  X(Uint8, 0x22, "uint8", 0, 0, false, false)                            \
  X(Varint32, 0x23, "varint32", 0, 0, false, false)                      \
  X(Varint64, 0x24, "varint64", 0, 0, false, false)                      \
  X(Varuint32, 0x25, "varuint32", 0, 0, false, false)                    \
  X(Varuint64, 0x26, "varuint64", 0, 0, false, false)                    \
  X(Opcode, 0x27, "opcode", 0, 0, true, false)                           \
  X(BinaryAccept, 0x28, "accept", 0, 0, false, false)                    \
  X(BinarySelect, 0x29, "binary", 0, 0, true, false)                     \
  X(BinaryEval, 0x2a, "opcode", 1, 0, true, false)                       \
  X(Bit, 0x2b, "bit", 0, 0, false, false)                                \
  /* Not an ast node, just for bit compression */                        \
  X(BinaryEvalBits, 0x2c, "opcode", 0, 0, false, false)                  \
                                                                         \
  /* Boolean Expressions */                                              \
  X(And, 0x30, "and", 2, 0, false, false)                                \
  X(Or, 0x31, "or", 2, 0, false, false)                                  \
  X(Not, 0x32, "not", 1, 0, false, false)                                \
  X(BitwiseAnd, 0x34, "bitwise.and", 2, 0, false, false)                 \
  X(BitwiseOr, 0x35, "bitwise.or", 2, 0, false, false)                   \
  X(BitwiseXor, 0x36, "bitwise.xor", 2, 0, false, false)                 \
  X(BitwiseNegate, 0x37, "bitwise.negate", 1, 0, false, false)           \
  X(LastSymbolIs, 0x38, "last.symbol.is", 1, 0, false, false)            \
                                                                         \
  /* I/O operations  */                                                  \
  X(Peek, 0x40, "peek", 1, 0, false, true)                               \
  X(Read, 0x41, "read", 1, 0, false, true)                               \
  X(LastRead, 0x42, "read", 0, 0, false, false)                          \
  X(Write, 0x43, "write", 1, 1, false, false)                            \
  X(Table, 0x44, "table", 1, 1, true, true)                              \
                                                                         \
  /* Other */                                                            \
  X(Param, 0x51, "param", 1, 0, false, false)                            \
  X(Local, 0x53, "local", 1, 0, false, false)                            \
  X(Set, 0x54, "set", 2, 0, false, false)                                \
  X(Map, 0x55, "map", 1, 0, true, false)                                 \
  X(Callback, 0x56, "=>", 1, 0, false, false)                            \
                                                                         \
  /* Declarations */                                                     \
  X(Define, 0x60, "define", 2, 1, true, true)                            \
  X(Algorithm, 0x62, "algorithm.node", 2, 0, false, false)               \
  X(Undefine, 0x64, "undefine", 1, 0, true, false)                       \
  X(LiteralDef, 0x65, "literal", 2, 0, false, false)                     \
  X(LiteralUse, 0x66, "literal.use", 1, 0, false, false)                 \
  X(Rename, 0x67, "rename", 2, 0, false, false)                          \
  X(Locals, 0x68, "locals", 1, 0, false, false)                          \
  X(ParamValues, 0x69, "values", 1, 0, false, false)                     \
  X(LiteralActionDef, 0x6a, "literal.action.define", 2, 0, false, false) \
  X(LiteralActionUse, 0x6b, "literal.action.use", 11, 0, false, false)   \
  X(LiteralActionBase, 0x6c, "literal.action.enum", 1, 0, false, false)  \
  X(SourceHeader, 0x77, "header", 0, 3, true, false)                     \
  X(ReadHeader, 0x78, "header.read", 0, 3, true, false)                  \
  X(WriteHeader, 0x79, "header.write", 0, 3, true, false)                \
  X(AlgorithmFlag, 0x7a, "algorithm", 1, 0, false, false)                \
  X(ParamExprs, 0x7b, "exprs", 1, 0, false, false)                       \
  X(ParamExprsCached, 0x7c, "exprs.cached", 1, 0, false, false)          \
  X(ParamCached, 0x7d, "cached", 0, 0, false, false)                     \
  X(ParamArgs, 0x7e, "args", 0, 2, false, false)                         \
  X(NoParams, 0x7f, "no.params", 0, 0, false, false)                     \
  X(NoLocals, 0x61, "no.locals", 0, 0, false, false)                     \
  X(AlgorithmName, 0x80, "name", 1, 0, false, false)                     \
  X(EnclosingAlgorithms, 0x81, "enclosing", 1, 5, false, false)          \
                                                                         \
  /* Internal (not opcodes in compressed file) */                        \
  X(UnknownSection, 0xFF, "unknown.section", 1, 0, true, false)          \
  X(SymbolDefn, 0x100, "symbol.defn", 0, 0, false, false)                \
  X(IntLookup, 0x101, "int.lookup", 0, 0, false, false)

#define ALGORITHM_DECLS                                          \
  VALIDATENODE                                                   \
 public:                                                         \
  const Header* getSourceHeader(bool UseEnclosing = true) const; \
  const Header* getReadHeader(bool UseEnclosing = true) const;   \
  const Header* getWriteHeader(bool UseEnclosing = true) const;  \
  const Symbol* getName() const;                                 \
  bool isAlgorithm() const;                                      \
  void init();                                                   \
  void clearCaches() { init(); }                                 \
                                                                 \
 private:                                                        \
  mutable const Header* SourceHdr;                               \
  mutable const Header* ReadHdr;                                 \
  mutable const Header* WriteHdr;                                \
  mutable const Symbol* Name;                                    \
  mutable bool IsAlgorithmSpecified;                             \
  mutable bool IsValidated;                                      \
  bool setIsAlgorithm(const Node* Nd);

#define DEFINE_DECLS                                                           \
  VALIDATENODE                                                                 \
 public:                                                                       \
  typedef std::vector<const Node*> ParamTypeVector;                            \
  DefineFrame* getDefineFrame() const;                                         \
  const std::string getName() const;                                           \
  size_t getNumLocals() const { return getDefineFrame()->getNumLocals(); }     \
  size_t getNumArgs() const { return getDefineFrame()->getNumArgs(); }         \
  size_t getNumValueArgs() const {                                             \
    return getDefineFrame()->getNumValueArgs();                                \
  }                                                                            \
  size_t getValueArg(size_t Index) const;                                      \
  size_t getNumExprArgs() const { return getDefineFrame()->getNumExprArgs(); } \
  bool isValidArgIndex(decode::IntType Index) const {                          \
    return Index < getDefineFrame()->getNumArgs();                             \
  }                                                                            \
  bool isValidLocalIndex(decode::IntType Index) const {                        \
    return Index < getDefineFrame()->getNumLocals();                           \
  }                                                                            \
  Node* getBody() const;                                                       \
                                                                               \
 private:                                                                      \
  friend class SymbolTable;                                                    \
  mutable std::unique_ptr<DefineFrame> MyDefineFrame;

#endif  // DECOMPRESSOR_SRC_SEXP_AST_DEFS_H_
