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

static inline Parser::symbol_type make_QuotedID(
    const Driver &Driver, const std::string &Name) {
  return Parser::make_QUOTED_IDENTIFIER(Name.substr(1, Name.size()-2),
                                        Driver.loc);
}

static uint64_t read_Integer(const Driver &Driver, const std::string &Name) {
  std::istringstream Strm(Name);
  uint64_t Value;
  Strm >> Value;
  if (!Strm.eof()) {
    driver.error("Malformed integer found");
  }
  return Value;
}

static Parser::symbol_type make_Integer(Driver &Driver,
                                        const std::string &Name) {
  IntData Data;
  Data.Name = Name;
  Data.Value = read_Integer(Driver, Name);
  return Parser::make_INTEGER(Data, driver.loc);
}

static Parser::symbol_type make_SignedInteger(
    const Driver &driver, const std::string &name) {
  IntData Data;
  Data.Name = Name;
  Data.Value = -static_cast<int64_t>(read_Integer(Driver, Name));
  return Parser::make_INTEGER(Data, driver.loc);
}

%}

%option noyywrap nounput batch debug noinput

blank	[ \t]
letter	[a-zA-Z]
digit	[0-9]
id	{letter}({letter}|{digit}|[_.])*

%{
// Code run each time a pattern is matched.
# define YY_USER_ACTION  driver.loc.columns (yyleng);
%}

%%

%{
  // Code run each time yylex is called.
  driver.loc.step ();
%}

{blank}+          driver.loc.step();
"#".*             driver.loc.step();
[\n]+             driver.loc.lines (yyleng); driver.loc.step ();
")"               return Parser::make_CLOSEPAREN(driver.loc);
"("               return Parser::make_OPENPAREN(driver.loc);
"bit.to.bit"      make_BIT_TO_BIT(driver.loc);
"bit.to.byte"     make_BIT_TO_BYTE(driver.loc);
"bit.to.int"      make_BIT_TO_INT(driver.loc);
"bit.to.ast"      make_BIT_TO_AST(driver.loc);
"byte.to.bit"     make_BYTE_TO_BIT(driver.loc);
"byte.to.byte"    make_BYTE_TO_BYTE(driver.loc);
"byte.to.int"     make_BYTE_TO_BIT(driver.loc);
"byte.to.ast"     make_BYTE_TO_AST(driver.loc);
"call"            make_CALL(driver.loc);
"case"            make_CASE(driver.loc);
"copy"            make_COPY(driver.loc);
"define"          make_DEFINE(driver.loc);
"eval"            make_EVAL(driver.loc);
"extract"         make_EXTRACT(driver.loc);
"filter"          make_FILTER(driver.loc);
"fixed32"         make_FIXED32(driver.loc);
"fixed64"         make_FIXED64(driver.loc);
"if"              make_IF(driver.loc);
"int.to.bit"      make_INT_TO_BIT(driver.loc);
"int.to.byte"     make_INT_TO_BYTE(driver.loc);
"int.to.int"      make_INT_TO_INT(driver.loc);
"int.to.ast"      make_INT_TO_AST(driver.loc);
"i32.const"       make_I32_CONST(driver.loc);
"i64.const"       make_I64_CONST(driver.loc);
"lit"             make_LIT(driver.loc);
"loop.unbounded"  make_LOOP_UNBOUNDED(driver.loc);
"loop"            make_LOOP(driver.loc);
"map"             make_MAP(driver.loc);
"method"          make_METHOD(driver.loc);
"peek"            make_PEEK(driver.loc);
"read"            make_READ(driver.loc);
"section"         make_SECTION(driver.loc);
"select"          make_SELECT(driver.loc);
"seq"             make_SEQ(driver.loc);
"sym.const"       make_SYM_CONST(driver.loc);
"uint8"           make_UINT8(driver.loc);
"uint32"          make_UINT32(driver.loc);
"uint64"          make_UINT64(driver.loc);
"u32.const"       make_U32_CONST(driver.loc);
"u64.const"       make_U64_CONST(driver.loc);
"value"           make_VALUE(driver.loc);
"varint32"        make_VARINT32(driver.loc);
"varint64"        make_VARINT64(driver.loc);
"varuint1"        make_VARUINT1(driver.loc);
"varuint7"        make_VARUINT7(driver.loc);
"varuint32"       make_VARUINT32(driver.loc);
"varuint64"       make_VARUINT64(driver.loc);
"vbrint32"        make_VBRINT32(driver.loc);
"vbrint64"        make_VBRINT32(driver.loc);
"vbruint32"       make_VBRUINT32(driver.loc);
"vbruint64"       make_VBRUINT32(driver.loc);
"version"         make_VERSION(driver.loc);
"void"            make_VOID(driver.loc);
"write"           make_WRITE(driver.loc);
{digit}+          return make_Integer(driver, yytext);
-?{digit}+        return make_SignedInteger(driver, yytext);
{id}              return make_IDENTIFIER(yytext, driver.loc);
"'"{id}"'"        return make_QuotedID(driver, yytext);
.                 driver.error("invalid character");
<<EOF>>           return Parser::make_END(driver.loc);
%%


namespace wasm {
namespace filt {
void Driver::scan_begin() {
  yy_flex_debug = trace_scanning;
  if (file.empty() || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen(file.c_str(), "r"))) {
    error("cannot open " + file + ": " + strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void Driver::scan_end () {
  fclose (yyin);
}

}}
