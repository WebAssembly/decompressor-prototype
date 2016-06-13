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

#include <iostream>

using namespace wasm::filt;

void usage(const char *AppName) {
  fprintf(stderr, "usage: %s [options] [Filename ...]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Parses filter files (stdin if none).\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -l\t\tTrace lexing of file(s).\n");
  fprintf(stderr, "  -p\t\tTrace parsing of file(s).\n");
  fprintf(stderr, "  -w\t\tWrite out parsed s-expressions.\n");
}

int main(int Argc, char *Argv[]) {
  Driver Driver;
  bool PrintAst = false;
  std::vector<const char *> Files;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("-p"))
      Driver.setTraceParsing(true);
    else if (Argv[i] == std::string("-l"))
      Driver.setTraceLexing(true);
    else if (Argv[i] == std::string("-w"))
      PrintAst = true;
    else if (Argv[i] == std::string("-h")
             || Argv[i] == std::string("--help")) {
      usage(Argv[i]);
      return EXIT_SUCCESS;
    } else
      Files.push_back(Argv[i]);
  }
  if (Files.empty())
    Files.push_back("-");
  for (const auto *Filename : Files) {
    if (Files.size() > 1) {
      fprintf(stdout, "Parsing: %s...\n", Filename);
    }
    if (!Driver.parse(Filename)) {
      fprintf(stderr, "Errors detected: %s\n", Filename);
      return EXIT_FAILURE;
    }
    if (PrintAst) {
      if (Node *Root = Driver.getParsedAst()) {
        TextWriter Writer;
        Writer.write(stdout, Root);
      } else {
        fprintf(stderr, "No filter s-expressions found: %s\n", Filename);
      }
    }
  }
  return EXIT_SUCCESS;
}
