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

%{

#include <sstream>
#include <string>

#include "Parser.tab.hpp"
#include "Driver.h"

using namespace wasm::filt;

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
# undef yywrap
# define yywrap() 1

static Parser::symbol_type make_QuotedID(
    const Driver &Driver, const std::string &Name) {
  // TODO(KarlSchimpf) allow more general forms (any sequence of uint8)?
  return Parser::make_IDENTIFIER(Name.substr(1, Name.size()-2),
                                 Driver.getLoc());
}

static uint64_t read_Integer(const Driver &Driver, const std::string &Name) {
  std::istringstream Strm(Name);
  uint64_t Value = 0;
  Strm >> Value;
  if (!Strm.eof())
    Driver.error("Malformed integer found");
  return Value;
}

static Parser::symbol_type make_Integer(Driver &Driver,
                                        const std::string &Name) {
  return Parser::make_INTEGER(read_Integer(Driver, Name), Driver.getLoc());
}

static Parser::symbol_type make_SignedInteger(
    const Driver &Driver, const std::string &Name) {
  return Parser::make_INTEGER(-static_cast<int64_t>(read_Integer(Driver, Name)),
                              Driver.getLoc());
}

static Parser::symbol_type make_HexInteger(
    const Driver &Driver, const std::string &Name) {
  uint64_t Value;
  std::istringstream Strm(Name);
  Strm >> std::hex >> Value;
  if (!Strm.eof())
    Driver.error("Malformed integer found");
  return Parser::make_INTEGER(Value, Driver.getLoc());
}

%}

%option noyywrap nounput batch debug noinput

blank	[ \t]
digit	[0-9]
hexdigit [0-9a-fA-F]
letter	[a-zA-Z]
id	{letter}({letter}|{digit}|[_.])*

%{
// Code run each time a pattern is matched.
# define YY_USER_ACTION  Driver.extendLocationColumns(yyleng);
%}

%%

%{
  // Code run each time yylex is called.
  Driver.stepLocation();
%}

{blank}+          Driver.stepLocation();
"#".*             Driver.stepLocation();
[\n]+             Driver.extendLocationLines(yyleng); Driver.stepLocation();
")"               return Parser::make_CLOSEPAREN(Driver.getLoc());
"("               return Parser::make_OPENPAREN(Driver.getLoc());
"append"          return Parser::make_APPEND(Driver.getLoc());
"ast.to.bit"      return Parser::make_AST_TO_BIT(Driver.getLoc());
"ast.to.byte"     return Parser::make_AST_TO_BYTE(Driver.getLoc());
"ast.to.int"      return Parser::make_AST_TO_INT(Driver.getLoc());
"bit.to.bit"      return Parser::make_BIT_TO_BIT(Driver.getLoc());
"bit.to.byte"     return Parser::make_BIT_TO_BYTE(Driver.getLoc());
"bit.to.int"      return Parser::make_BIT_TO_INT(Driver.getLoc());
"bit.to.ast"      return Parser::make_BIT_TO_AST(Driver.getLoc());
"block"           return Parser::make_BLOCK(Driver.getLoc());
"block.begin"     return Parser::make_BLOCKBEGIN(Driver.getLoc());
"block.end"       return Parser::make_BLOCKEND(Driver.getLoc());
"byte.to.bit"     return Parser::make_BYTE_TO_BIT(Driver.getLoc());
"byte.to.byte"    return Parser::make_BYTE_TO_BYTE(Driver.getLoc());
"byte.to.int"     return Parser::make_BYTE_TO_BIT(Driver.getLoc());
"byte.to.ast"     return Parser::make_BYTE_TO_AST(Driver.getLoc());
"case"            return Parser::make_CASE(Driver.getLoc());
"copy"            return Parser::make_COPY(Driver.getLoc());
"default"         return Parser::make_DEFAULT(Driver.getLoc());
"define"          return Parser::make_DEFINE(Driver.getLoc());
"eval"            return Parser::make_EVAL(Driver.getLoc());
"filter"          return Parser::make_FILTER(Driver.getLoc());
"if"              return Parser::make_IF(Driver.getLoc());
"int.to.bit"      return Parser::make_INT_TO_BIT(Driver.getLoc());
"int.to.byte"     return Parser::make_INT_TO_BYTE(Driver.getLoc());
"int.to.int"      return Parser::make_INT_TO_INT(Driver.getLoc());
"int.to.ast"      return Parser::make_INT_TO_AST(Driver.getLoc());
"i32.const"       return Parser::make_I32_CONST(Driver.getLoc());
"i64.const"       return Parser::make_I64_CONST(Driver.getLoc());
"lit"             return Parser::make_LIT(Driver.getLoc());
"loop.unbounded"  return Parser::make_LOOP_UNBOUNDED(Driver.getLoc());
"loop"            return Parser::make_LOOP(Driver.getLoc());
"map"             return Parser::make_MAP(Driver.getLoc());
"peek"            return Parser::make_PEEK(Driver.getLoc());
"postorder"       return Parser::make_POSTORDER(Driver.getLoc());
"preorder"        return Parser::make_PREORDER(Driver.getLoc());
"read"            return Parser::make_READ(Driver.getLoc());
"section"         return Parser::make_SECTION(Driver.getLoc());
"select"          return Parser::make_SELECT(Driver.getLoc());
"seq"             return Parser::make_SEQ(Driver.getLoc());
"sym.const"       return Parser::make_SYM_CONST(Driver.getLoc());
"uint8"           return Parser::make_UINT8(Driver.getLoc());
"uint32"          return Parser::make_UINT32(Driver.getLoc());
"uint64"          return Parser::make_UINT64(Driver.getLoc());
"undefine"        return Parser::make_UNDEFINE(Driver.getLoc());
"u32.const"       return Parser::make_U32_CONST(Driver.getLoc());
"u64.const"       return Parser::make_U64_CONST(Driver.getLoc());
"value"           return Parser::make_VALUE(Driver.getLoc());
"varint32"        return Parser::make_VARINT32(Driver.getLoc());
"varint64"        return Parser::make_VARINT64(Driver.getLoc());
"varuint1"        return Parser::make_VARUINT1(Driver.getLoc());
"varuint7"        return Parser::make_VARUINT7(Driver.getLoc());
"varuint32"       return Parser::make_VARUINT32(Driver.getLoc());
"varuint64"       return Parser::make_VARUINT64(Driver.getLoc());
"version"         return Parser::make_VERSION(Driver.getLoc());
"void"            return Parser::make_VOID(Driver.getLoc());
"write"           return Parser::make_WRITE(Driver.getLoc());
"0x"{hexdigit}+   return make_HexInteger(Driver, yytext);
{digit}+          return make_Integer(Driver, yytext);
-?{digit}+        return make_SignedInteger(Driver, yytext);
"'"{id}"'"        return make_QuotedID(Driver, yytext);
.                 Driver.error("invalid character");
<<EOF>>           return Parser::make_END(Driver.getLoc());
%%


namespace wasm {
namespace filt {

void Driver::Begin() {
  yy_flex_debug = TraceLexing;
  if (Filename.empty() || Filename == "-")
    yyin = stdin;
  else if (!(yyin = fopen(Filename.c_str(), "r"))) {
    error("cannot open " + Filename + ": " + strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void Driver::End() {
  if (!Filename.empty() && Filename != "-")
    fclose(yyin);
}

} // end of namespace filt

} // end of namespace wasm
