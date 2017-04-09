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

/* Defines a driver class to lex/parse filter s-expressions */

#ifndef DECOMPRESSOR_PROTOTYPE_SRC_SEXP_PARSER_DRIVER_H
#define DECOMPRESSOR_PROTOTYPE_SRC_SEXP_PARSER_DRIVER_H

#include "sexp-parser/Parser.tab.hpp"

#include <map>
#include <memory>
#include <string>

namespace wasm {
namespace filt {

// Driver connecting Parser, Lexer, Positions, and Locations.
class Driver {
  Driver(const Driver&) = delete;
  Driver& operator=(const Driver&) = delete;

 public:
  Driver(std::shared_ptr<SymbolTable> Table)
      : Table(Table),
        TraceLexing(false),
        TraceParsing(false),
        ParsedAst(nullptr),
        ErrorsReported(false) {}

  ~Driver() {}

  template <typename T>
  T* create() {
    return Table->create<T>();
  }
  template <typename T>
  T* create(Node* Nd) {
    return Table->create<T>(Nd);
  }
  template <typename T>
  T* create(Node* Nd1, Node* Nd2) {
    return Table->create<T>(Nd1, Nd2);
  }
  template <typename T>
  T* create(Node* Nd1, Node* Nd2, Node* Nd3) {
    return Table->create<T>(Nd1, Nd2, Nd3);
  }
  BinaryAcceptNode* createBinaryAccept(decode::IntType Value,
                                       unsigned NumBits) {
    return Table->createBinaryAccept(Value, NumBits);
  }

#define X(tag, format, defval, mergable, NODE_DECLS)               \
  tag##Node* getOrCreate##tag(                                     \
      decode::IntType Value,                                       \
      decode::ValueFormat Format = decode::ValueFormat::Decimal) { \
    return Table->getOrCreate##tag(Value, Format);                 \
  }                                                                \
  tag##Node* getOrCreate##tag() { return Table->getOrCreate##tag(); }
  AST_INTEGERNODE_TABLE
#undef X

  SymbolNode* getOrCreateSymbol(std::string& Name) {
    return Table->getOrCreateSymbol(Name);
  }

  void appendArgument(Node* Nd, Node* Arg);

  // The name of the file being parsed.
  std::string& getFilename() { return Filename; }

  void setTraceLexing(bool NewValue) { TraceLexing = NewValue; }

  void setTraceParsing(bool NewValue) { TraceParsing = NewValue; }

  const location& getLoc() const { return Loc; }

  void stepLocation() { Loc.step(); }

  void extendLocationColumns(size_t NumColumns) { Loc.columns(NumColumns); }

  void extendLocationLines(size_t NumLines) { Loc.lines(NumLines); }

  // Run the parser on file F. Returns true on success.
  bool parse(const std::string& Filename);

  // Returns the last parsed ast.
  Node* getParsedAst() const { return ParsedAst; }

  SymbolTable::SharedPtr getSymbolTable() const { return Table; }

  bool install() { return Table->install(); }

  void setParsedAst(Node* Ast) {
    ParsedAst = Ast;
    Table->setRoot(dyn_cast<FileNode>(ParsedAst));
  }

  // Error handling.
  enum class ErrorLevel { Warn, Error, Fatal };
  static const char* getName(ErrorLevel Level);
  void report(ErrorLevel Level,
              const wasm::filt::location& Loc,
              const std::string& Message);
  void report(ErrorLevel Level, const std::string& Message) {
    report(Level, Loc, Message);
  }
  void warn(const wasm::filt::location& Loc, const std::string& Message) {
    report(ErrorLevel::Warn, Loc, Message);
  }
  void warn(const std::string& Message) { report(ErrorLevel::Warn, Message); }
  void error(const wasm::filt::location& Loc, const std::string& Message) {
    report(ErrorLevel::Error, Loc, Message);
  }
  void error(const std::string& Message) { report(ErrorLevel::Error, Message); }
  void fatal(const wasm::filt::location& Loc, const std::string& Message) {
    report(ErrorLevel::Fatal, Loc, Message);
  }
  void fatal(const std::string& Message) { report(ErrorLevel::Fatal, Message); }
  void tokenError(const std::string& Token);

 private:
  std::shared_ptr<SymbolTable> Table;
  std::string Filename;
  bool TraceLexing;
  bool TraceParsing;
  bool MaintainIntegerFormatting;
  // The location of the last token.
  location Loc;
  Node* ParsedAst;
  bool ErrorsReported;

  // Called before parsing for setup.
  void Begin();
  // Called after parsing for cleanup.
  void End();
};

}  // end of namespace filt

}  // end of namespace filt

// Tell Flex the lexer's prototype ...
#define YY_DECL \
  wasm::filt::Parser::symbol_type yylex(wasm::filt::Driver& Driver)
// ... and declare it for the parser's sake.
YY_DECL;

#endif  // DECOMPRESSOR_PROTOTYPE_SRC_SEXP_PARSER_DRIVER_H
