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

// Reads a filter file.

#include "sexp/TextWriter.h"
#include "sexp-parser/Parser.tab.hpp"
#include "sexp-parser/Driver.h"
#include "utils/ArgsParse.h"
#include "utils/Defs.h"

#include <iostream>

using namespace wasm::filt;
using namespace wasm::decode;
using namespace wasm::utils;

int main(int Argc, wasm::charstring Argv[]) {
  bool PrintAst = false;
  std::vector<wasm::charstring> Files;
  bool TraceParser = false;
  bool TraceLexer = false;
  bool TraceFilesParsed = false;
  bool ValidateAst = false;

  {
    ArgsParser Args("Parses algorithm files");

    ArgsParser::Toggle ExpectExitFailFlag(ExpectExitFail);
    Args.add(ExpectExitFailFlag.setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success."));

    ArgsParser::RequiredVector<wasm::charstring> FilesFlag(Files);
    Args.add(FilesFlag.setOptionName("INPUT")
                 .setDescription("Input file to parse."));

    ArgsParser::Toggle TraceLexerFlag(TraceLexer);
    Args.add(TraceLexerFlag.setLongName("verbose=lexer")
                 .setDescription("Trace lexing file(s)."));

    ArgsParser::Toggle TraceParserFlag(TraceParser);
    Args.add(TraceParserFlag.setLongName("verbose=parser")
                 .setDescription("Trace parsing file(s)."));

    ArgsParser::Toggle TracePrintAstFlag(PrintAst);
    Args.add(TracePrintAstFlag.setShortName('p')
                 .setLongName("print")
                 .setDescription("Write out parsed s-expression"));

    ArgsParser::Toggle TraceFilesParsedFlag(TraceFilesParsed);
    Args.add(TraceFilesParsedFlag.setShortName('v')
                 .setLongName("verbose")
                 .setDescription("Show file(s) being parsed."));

    ArgsParser::Toggle ShowInternalStructureFlag(
        TextWriter::DefaultShowInternalStructure);
    Args.add(
        ShowInternalStructureFlag.setShortName('s')
            .setLongName("structure")
            .setDescription(
                "Show internal structure of how algorithms are represented "
                "when printing."));

    ArgsParser::Toggle ValidateAstFlag(ValidateAst);
    Args.add(ValidateAstFlag.setLongName("validate")
                 .setDescription(
                     "Validate parsed algorithms also. Assumes "
                     "order of input files define enclosing scopes."));

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }
  }

  Driver Driver(std::make_shared<SymbolTable>());
  Driver.setTraceParsing(TraceParser);
  Driver.setTraceLexing(TraceLexer);
  Driver.setTraceFilesParsed(TraceFilesParsed);
  if (Files.empty())
    Files.push_back("-");

  SymbolTable::SharedPtr ContextSymtab;
  for (const auto* Filename : Files) {
    if (!TraceFilesParsed && Files.size() > 1) {
      fprintf(stdout, "Parsing: %s...\n", Filename);
    }
    if (!Driver.parse(Filename)) {
      fprintf(stderr, "Errors detected while parsing: %s\n", Filename);
      return exit_status(EXIT_FAILURE);
    }
    if (ValidateAst) {
      SymbolTable::SharedPtr Symtab = Driver.getSymbolTable();
      Symtab->setEnclosingScope(ContextSymtab);
      if (!Symtab->install()) {
        fprintf(stderr, "Errors detected while validating: %s\n", Filename);
        return exit_status(EXIT_FAILURE);
      }
      ContextSymtab = Symtab;
    }
    if (PrintAst) {
      if (Node* Root = Driver.getParsedAst()) {
        TextWriter Writer;
        Writer.write(stdout, Root);
      } else {
        fprintf(stderr, "No filter s-expressions found: %s\n", Filename);
      }
    }
  }
  return exit_status(EXIT_SUCCESS);
}
