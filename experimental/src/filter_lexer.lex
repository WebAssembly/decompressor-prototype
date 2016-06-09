%{ /* -*- C++ -*- */

#include <sstream>
#include <string>

#include "filter_ast.h"
#include "FilterParser.tab.hpp"
#include "FilterDriver.h"

using namespace wasm::filt;

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
# undef yywrap
# define yywrap() 1

static FilterParser::symbol_type make_KEYWORD_or_ID(
    const FilterDriver &driver, const std::string &name) {
  int Index = driver.getKeywordIndex(name);
  switch (Index) {
    default:
      return FilterParser::make_IDENTIFIER(name, driver.loc);
    case FilterParser::token::AST:
      return FilterParser::make_AST(driver.loc);
    case FilterParser::token::BIT:
      return FilterParser::make_BIT(driver.loc);
    case FilterParser::token::BYTE:
      return FilterParser::make_BYTE(driver.loc);
    case FilterParser::token::CALL:
      return FilterParser::make_CALL(driver.loc);
    case FilterParser::token::CASE:
      return FilterParser::make_CASE(driver.loc);
    case FilterParser::token::COPY:
      return FilterParser::make_COPY(driver.loc);
    case FilterParser::token::DEFAULT:
      return FilterParser::make_DEFAULT(driver.loc);
    case FilterParser::token::DEFINE:
      return FilterParser::make_DEFINE(driver.loc);
    case FilterParser::token::ELSE:
      return FilterParser::make_ELSE(driver.loc);
    case FilterParser::token::EVAL:
      return FilterParser::make_EVAL(driver.loc);
    case FilterParser::token::EXTRACT:
      return FilterParser::make_EXTRACT(driver.loc);
    case FilterParser::token::FILTER:
      return FilterParser::make_FILTER(driver.loc);
    case FilterParser::token::FIXED:
      return FilterParser::make_FIXED(driver.loc);
    case FilterParser::token::IF:
      return FilterParser::make_IF(driver.loc);
    case FilterParser::token::INT:
      return FilterParser::make_INT(driver.loc);
    case FilterParser::token::IVBR32:
      return FilterParser::make_IVBR32(driver.loc);
    case FilterParser::token::IVBR64:
      return FilterParser::make_IVBR64(driver.loc);
    case FilterParser::token::I32_CONST:
      return FilterParser::make_I32_CONST(driver.loc);
    case FilterParser::token::I64_CONST:
      return FilterParser::make_I64_CONST(driver.loc);
    case FilterParser::token::LIT:
      return FilterParser::make_LIT(driver.loc);
    case FilterParser::token::LOOP:
      return FilterParser::make_LOOP(driver.loc);
    case FilterParser::token::MAP:
      return FilterParser::make_MAP(driver.loc);
    case FilterParser::token::METHOD:
      return FilterParser::make_METHOD(driver.loc);
    case FilterParser::token::PEEK:
      return FilterParser::make_PEEK(driver.loc);
    case FilterParser::token::READ:
      return FilterParser::make_READ(driver.loc);
    case FilterParser::token::SECTION:
      return FilterParser::make_SECTION(driver.loc);
    case FilterParser::token::SELECT:
      return FilterParser::make_SELECT(driver.loc);
    case FilterParser::token::SYM_CONST:
      return FilterParser::make_SYM_CONST(driver.loc);
    case FilterParser::token::UINT8:
      return FilterParser::make_UINT8(driver.loc);
    case FilterParser::token::UINT32:
      return FilterParser::make_UINT32(driver.loc);
    case FilterParser::token::UINT64:
      return FilterParser::make_UINT64(driver.loc);
    case FilterParser::token::U32_CONST:
      return FilterParser::make_U32_CONST(driver.loc);
    case FilterParser::token::U64_CONST:
      return FilterParser::make_U64_CONST(driver.loc);
    case FilterParser::token::VALUE:
      return FilterParser::make_VALUE(driver.loc);
    case FilterParser::token::VARINT32:
      return FilterParser::make_VARINT32(driver.loc);
    case FilterParser::token::VARINT64:
      return FilterParser::make_VARINT64(driver.loc);
    case FilterParser::token::VARUINT1:
      return FilterParser::make_VARUINT1(driver.loc);
    case FilterParser::token::VARUINT7:
      return FilterParser::make_VARUINT7(driver.loc);
    case FilterParser::token::VARUINT32:
      return FilterParser::make_VARUINT32(driver.loc);
    case FilterParser::token::VARUINT64:
      return FilterParser::make_VARUINT64(driver.loc);
    case FilterParser::token::VBR32:
      return FilterParser::make_VBR32(driver.loc);
    case FilterParser::token::VBR64:
      return FilterParser::make_VBR64(driver.loc);
    case FilterParser::token::VERSION:
      return FilterParser::make_VERSION(driver.loc);
    case FilterParser::token::VOID:
      return FilterParser::make_VOID(driver.loc);
    case FilterParser::token::WRITE:
      return FilterParser::make_WRITE(driver.loc);
  }
}

static inline FilterParser::symbol_type make_QuotedID(
    const FilterDriver &driver, const std::string &name) {
  return FilterParser::make_QUOTED_IDENTIFIER(name.substr(1, name.size()-2),
                                              driver.loc);
}

static FilterParser::symbol_type make_Integer(FilterDriver &driver,
                                              const std::string &Name) {
  IntData Data;
  Data.Name = Name;
  std::istringstream Strm(Name);
  Strm >> Data.Value;
  if (!Strm.eof()) {
    driver.error("Malformed integer found");
  }
  return FilterParser::make_INTEGER(Data, driver.loc);
}

// }}

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

{blank}+   driver.loc.step();
[\n]+      driver.loc.lines (yyleng); driver.loc.step ();
","        return FilterParser::make_COMMA(driver.loc);
":"        return FilterParser::make_COLON(driver.loc);
";"        return FilterParser::make_SEMICOLON(driver.loc);
"{"        return FilterParser::make_OPENBLOCK(driver.loc);
"("        return FilterParser::make_OPENPAREN(driver.loc);
"}"        return FilterParser::make_CLOSEBLOCK(driver.loc);
")"        return FilterParser::make_CLOSEPAREN(driver.loc);
"->"       return FilterParser::make_ARROW(driver.loc);
{digit}+   return make_Integer(driver, yytext);
{id}       return make_KEYWORD_or_ID(driver, yytext);
"'"{id}"'" return make_QuotedID(driver, yytext);
.          driver.error("invalid character");
<<EOF>>    return FilterParser::make_END(driver.loc);
%%


namespace wasm {
namespace filt {
void FilterDriver::scan_begin() {
  yy_flex_debug = trace_scanning;
  if (file.empty() || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen(file.c_str(), "r"))) {
    error("cannot open " + file + ": " + strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void FilterDriver::scan_end () {
  fclose (yyin);
}

}}
