#include "FilterParser.tab.hpp"
#include "FilterDriver.h"

namespace wasm {
namespace filt {

FilterDriver::FilterDriver() {
  KeywordIndex["ast"] = wasm::filt::FilterParser::token::AST;
  KeywordIndex["bit"] = wasm::filt::FilterParser::token::BIT;
  KeywordIndex["byte"] = wasm::filt::FilterParser::token::BYTE;
  KeywordIndex["call"] = wasm::filt::FilterParser::token::CALL;
  KeywordIndex["case"] = wasm::filt::FilterParser::token::CASE;
  KeywordIndex["copy"] = wasm::filt::FilterParser::token::COPY;
  KeywordIndex["default"] = wasm::filt::FilterParser::token::DEFAULT;
  KeywordIndex["define"] = wasm::filt::FilterParser::token::DEFINE;
  KeywordIndex["else"] = wasm::filt::FilterParser::token::ELSE;
  KeywordIndex["eval"] = wasm::filt::FilterParser::token::EVAL;
  KeywordIndex["extract"] = wasm::filt::FilterParser::token::EXTRACT;
  KeywordIndex["filter"] = wasm::filt::FilterParser::token::FILTER;
  KeywordIndex["fixed"] = wasm::filt::FilterParser::token::FIXED;
  KeywordIndex["if"] = wasm::filt::FilterParser::token::IF;
  KeywordIndex["int"] = wasm::filt::FilterParser::token::INT;
  KeywordIndex["ivbr32"] = wasm::filt::FilterParser::token::IVBR32;
  KeywordIndex["ivbr64"] = wasm::filt::FilterParser::token::IVBR64;
  KeywordIndex["i32.const"] = wasm::filt::FilterParser::token::I32_CONST;
  KeywordIndex["i64.const"] = wasm::filt::FilterParser::token::I64_CONST;
  KeywordIndex["lit"] = wasm::filt::FilterParser::token::LIT;
  KeywordIndex["loop"] = wasm::filt::FilterParser::token::LOOP;
  KeywordIndex["map"] = wasm::filt::FilterParser::token::MAP;
  KeywordIndex["method"] = wasm::filt::FilterParser::token::METHOD;
  KeywordIndex["peek"] = wasm::filt::FilterParser::token::PEEK;
  KeywordIndex["read"] = wasm::filt::FilterParser::token::READ;
  KeywordIndex["section"] = wasm::filt::FilterParser::token::SECTION;
  KeywordIndex["select"] = wasm::filt::FilterParser::token::SELECT;
  KeywordIndex["sym.const"] = wasm::filt::FilterParser::token::SYM_CONST;
  KeywordIndex["uint8"] = wasm::filt::FilterParser::token::UINT8;
  KeywordIndex["uint32"] = wasm::filt::FilterParser::token::UINT32;
  KeywordIndex["uint64"] = wasm::filt::FilterParser::token::UINT64;
  KeywordIndex["u32.const"] = wasm::filt::FilterParser::token::U32_CONST;
  KeywordIndex["u64.const"] = wasm::filt::FilterParser::token::U64_CONST;
  KeywordIndex["value"] = wasm::filt::FilterParser::token::VALUE;
  KeywordIndex["varint32"] = wasm::filt::FilterParser::token::VARINT32;
  KeywordIndex["varint64"] = wasm::filt::FilterParser::token::VARINT64;
  KeywordIndex["varuint1"] = wasm::filt::FilterParser::token::VARUINT1;
  KeywordIndex["varuint7"] = wasm::filt::FilterParser::token::VARUINT7;
  KeywordIndex["varuint32"] = wasm::filt::FilterParser::token::VARUINT32;
  KeywordIndex["varuint64"] = wasm::filt::FilterParser::token::VARUINT64;
  KeywordIndex["vbr32"] = wasm::filt::FilterParser::token::VBR32;
  KeywordIndex["vbr64"] = wasm::filt::FilterParser::token::VBR64;
  KeywordIndex["version"] = wasm::filt::FilterParser::token::VERSION;
  KeywordIndex["void"] = wasm::filt::FilterParser::token::VOID;
  KeywordIndex["write"] = wasm::filt::FilterParser::token::WRITE;
}

int FilterDriver::getKeywordIndex(const std::string &name) const {
  const auto pos = KeywordIndex.find(name);
  if (pos == KeywordIndex.end())
    return wasm::filt::FilterParser::token::IDENTIFIER;
  return pos->second;
}


int
FilterDriver::parse (const std::string &f)
{
  file = f;
  scan_begin ();
  wasm::filt::FilterParser parser (*this);
  parser.set_debug_level (trace_parsing);
  int res = parser.parse ();
  scan_end ();
  return res;
}

void
FilterDriver::error (const wasm::filt::location& l, const std::string& m)
{
  std::cerr << l << ": " << m << std::endl;
}

void
FilterDriver::error (const std::string& m)
{
  error(loc, m);
}

}}
