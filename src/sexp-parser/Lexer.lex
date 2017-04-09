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

#include <string>

#include "sexp-parser/Parser.tab.hpp"
#include "sexp-parser/Driver.h"

using namespace wasm::decode;
using namespace wasm::filt;

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
# undef yywrap
# define yywrap() 1

// Override failure.
#define YY_FATAL_ERROR(msg) LexDriver->fatal(msg)

static Driver *LexDriver = nullptr;
static std::string Buffer;

static void buffer_Text(const std::string Text) {
  Buffer.append(Text);
}

static void buffer_Escape(const std::string Text) {
  assert(Text.size() == 1);
  switch (Text[0]) {
    case '\\':
      Buffer.push_back('\\');
      return;
    case 'f':
      Buffer.push_back('\f');
      return;
    case 'n':
      Buffer.push_back('\n');
      return;
    case 'r':
      Buffer.push_back('\r');
      return;
    case 't':
      Buffer.push_back('\t');
      return;
    case 'v':
      Buffer.push_back('\v');
      return;
    default:
      Buffer.push_back(Text[0]);
      return;
  }
}

static void buffer_Control(const std::string Text) {
  assert(Text.size() == 1);
  switch (Text[0]) {
    case 'f':
      Buffer.push_back('\f');
      return;
    case 'n':
      Buffer.push_back('\n');
      return;
    case 'r':
      Buffer.push_back('\r');
      return;
    case 't':
      Buffer.push_back('\t');
      return;
    case 'v':
      Buffer.push_back('\v');
      return;
    default:
      assert(false);
      buffer_Escape(Text);
      return;
  }
}

static void buffer_Octal(const std::string Text) {
  uint8_t Value = 0;
  for (uint8_t Ch : Text)
    Value = (Value << 3) + (Ch - '0');
  Buffer.push_back(Value);
}

static IntType read_Integer(const std::string &Name, size_t StartIndex = 0) {
  IntType Value = 0;
  for (size_t i = StartIndex, Size = Name.size(); i < Size; ++i) {
    char ch = Name[i];
    Value = (Value * 10) + uint8_t(ch - '0');
  }
  return Value;
}

static Parser::symbol_type make_Integer(Driver &Driver,
                                        const std::string &Name) {
  IntValue Value;
  Value.Value = read_Integer(Name);
  Value.Format = ValueFormat::Decimal;
  return Parser::make_INTEGER(Value, Driver.getLoc());
}

static Parser::symbol_type make_SignedInteger(Driver &Driver,
                                              const std::string &Name) {
  IntType UnsignedValue = read_Integer(Name, 1);
  IntValue Value;
  Value.Value = IntType(- SignedIntType(UnsignedValue));
  Value.Format = ValueFormat::SignedDecimal;
  return Parser::make_INTEGER(Value, Driver.getLoc());
}

static uint8_t getHex(char Ch) {
  switch (Ch) {
    default:
      assert(false && "getHex of non-hex character");
      return 0;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return Ch - '0';
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      return 10 + (Ch - 'a');
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
      return 10 + (Ch - 'A');
  }
}

static Parser::symbol_type make_HexInteger(Driver &Driver,
                                           const std::string &Name) {
  IntType HexValue = 0;
  assert(Name[0] == '0');
  assert(Name[1] == 'x');
  for (size_t i = 2, Count = Name.size(); i < Count; ++i) {
    HexValue = (HexValue << 4) + getHex(Name[i]);
  }
  IntValue Value;
  Value.Value = HexValue;
  Value.Format = ValueFormat::Hexidecimal;
  return Parser::make_INTEGER(Value, Driver.getLoc());
}

//octalchar "\\"[0-3][0-7][0-7]
//escapechar "\\\\"|"\\f"|"\\m"|"\\r"|"\\t"|"\\v"

%}

%option noyywrap nounput batch debug noinput

blank	[ \t]
digit	[0-9]
escape  "\\"[fnrtv\\]
hexdigit [0-9a-fA-F]
letter	[a-zA-Z]
/* id ({letter}|{digit}|[_.])* */

%{
// Code run each time a pattern is matched.
# define YY_USER_ACTION  Driver.extendLocationColumns(yyleng);
%}

%x Name
%x Escape

%%

%{
  // Code run each time yylex is called.
  Driver.stepLocation();
%}

{blank}+          Driver.stepLocation();
"#".*             Driver.stepLocation();
[\n]+             Driver.extendLocationLines(yyleng); Driver.stepLocation();
")"               return Parser::make_CLOSEPAREN(Driver.getLoc());
":"               return Parser::make_COLON(Driver.getLoc());
"("               return Parser::make_OPENPAREN(Driver.getLoc());
"."               return Parser::make_DOT(Driver.getLoc());
"accept"          return Parser::make_ACCEPT(Driver.getLoc());
"action"          return Parser::make_ACTION(Driver.getLoc());
"and"             return Parser::make_AND(Driver.getLoc());
"binary"          return Parser::make_BINARY(Driver.getLoc());
"bit"             return Parser::make_BIT(Driver.getLoc());
"block"           return Parser::make_BLOCK(Driver.getLoc());
"bitwise"         return Parser::make_BITWISE(Driver.getLoc());
"case"            return Parser::make_CASE(Driver.getLoc());
"declarations"    return Parser::make_DECLARATIONS(Driver.getLoc());
"define"          return Parser::make_DEFINE(Driver.getLoc());
"enum"            return Parser::make_ENUM(Driver.getLoc());
"enclosing"       return Parser::make_ENCLOSING(Driver.getLoc());
"error"           return Parser::make_ERROR(Driver.getLoc());
"eval"            return Parser::make_EVAL(Driver.getLoc());
"header"          return Parser::make_HEADER(Driver.getLoc());
"if"              return Parser::make_IF(Driver.getLoc());
"is"              return Parser::make_IS(Driver.getLoc());
"i32.const"       return Parser::make_I32_CONST(Driver.getLoc());
"i64.const"       return Parser::make_I64_CONST(Driver.getLoc());
"literal"         return Parser::make_LITERAL(Driver.getLoc());
"last"            return Parser::make_LAST(Driver.getLoc());
"local"           return Parser::make_LOCAL(Driver.getLoc());
"locals"          return Parser::make_LOCALS(Driver.getLoc());
"loop.unbounded"  return Parser::make_LOOP_UNBOUNDED(Driver.getLoc());
"loop"            return Parser::make_LOOP(Driver.getLoc());
"map"             return Parser::make_MAP(Driver.getLoc());
"negate"          return Parser::make_NEGATE(Driver.getLoc());
"not"             return Parser::make_NOT(Driver.getLoc());
"opcode"          return Parser::make_OPCODE(Driver.getLoc());
"or"              return Parser::make_OR(Driver.getLoc());
"param"           return Parser::make_PARAM(Driver.getLoc());
"params"          return Parser::make_PARAMS(Driver.getLoc());
"peek"            return Parser::make_PEEK(Driver.getLoc());
"read"            return Parser::make_READ(Driver.getLoc());
"rename"          return Parser::make_RENAME(Driver.getLoc());
"seq"             return Parser::make_SEQ(Driver.getLoc());
"set"             return Parser::make_SET(Driver.getLoc());
"switch"          return Parser::make_SWITCH(Driver.getLoc());
"symbol"          return Parser::make_SYMBOL(Driver.getLoc());
"table"           return Parser::make_TABLE(Driver.getLoc());
"uint8"           return Parser::make_UINT8(Driver.getLoc());
"uint32"          return Parser::make_UINT32(Driver.getLoc());
"uint64"          return Parser::make_UINT64(Driver.getLoc());
"undefine"        return Parser::make_UNDEFINE(Driver.getLoc());
"u8.const"        return Parser::make_U8_CONST(Driver.getLoc());
"u32.const"       return Parser::make_U32_CONST(Driver.getLoc());
"u64.const"       return Parser::make_U64_CONST(Driver.getLoc());
"varint32"        return Parser::make_VARINT32(Driver.getLoc());
"varint64"        return Parser::make_VARINT64(Driver.getLoc());
"varuint32"       return Parser::make_VARUINT32(Driver.getLoc());
"varuint64"       return Parser::make_VARUINT64(Driver.getLoc());
"void"            return Parser::make_VOID(Driver.getLoc());
"write"           return Parser::make_WRITE(Driver.getLoc());
"xor"             return Parser::make_XOR(Driver.getLoc());
"=>"              return Parser::make_DOUBLE_ARROW(Driver.getLoc());
"0x"{hexdigit}+   return make_HexInteger(Driver, yytext);
{digit}+          return make_Integer(Driver, yytext);
-?{digit}+        return make_SignedInteger(Driver, yytext);
{letter}({letter}|{digit})+ ; Driver.tokenError(yytext);
"'"               {
                    Buffer.clear();
                    BEGIN(Name);
                  }
<Name>"\\"        BEGIN(Escape);
<Name>"'"         {
                    BEGIN(INITIAL);
                    return Parser::make_IDENTIFIER(Buffer, Driver.getLoc());
                  }
<Name>.           buffer_Text(yytext);
<Name>\n          {
                    Driver.tokenError(yytext);
                    BEGIN(INITIAL);
                  }
<Escape>[f,n,r,t,v] {
                    buffer_Control(yytext);
                    BEGIN(Name);
                  }
<Escape>[0-3][0-7][0-7] {
                    buffer_Octal(yytext);
                    BEGIN(Name);
                  }
<Escape>.         {
                     buffer_Escape(yytext);
                     BEGIN(Name);
                  }
.                 Driver.tokenError(yytext);
<<EOF>>           return Parser::make_END(Driver.getLoc());
%%


namespace wasm {
namespace filt {

void Driver::Begin() {
  // Force reference to overwritten yy_fatal_error, to get rid
  // of warning message.
  if (0)
    yy_fatal_error("");
  LexDriver = this;
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
