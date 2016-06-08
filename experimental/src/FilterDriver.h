#ifndef DECOMP_SRC_FILTER_DRIVER_H
#define DECOMP_SRC_FILTER_DRIVER_H

#include <map>
#include <string>

namespace wasm {
namespace filt {

// Conducting the whole scanning and parsing of filter definitions.
class FilterDriver
{
public:
  FilterDriver ();
  virtual ~FilterDriver () {}

  std::map<std::string, int> variables;

  int result;

  // Handling the scanner.
  void scan_begin ();
  void scan_end ();
  bool trace_scanning = false;

  // Run the parser on file F.
  // Return 0 on success.
  int parse (const std::string& f);
  // The name of the file being parsed.
  // Used later to pass the file name to the location tracker.
  std::string file;
  // Whether parser traces should be generated.
  bool trace_parsing = false;
  // Token value associated with each keyword.
  std::map<std::string, int> KeywordIndex;
  // Returns 0 if not keyword.
  int getKeywordIndex(const std::string& name) const;
  // The location of the last token.
  location loc;

  // Error handling.
  void error (const wasm::filt::location& l, const std::string& m);
  void error (const std::string& m);
};

}}

// Tell Flex the lexer's prototype ...
# define YY_DECL \
  wasm::filt::FilterParser::symbol_type yylex (wasm::filt::FilterDriver& driver)
// ... and declare it for the parser's sake.
YY_DECL;

#endif //  DECOMP_SRC_FILTER_DRIVER_H
