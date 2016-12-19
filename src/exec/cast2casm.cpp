// -*- C++ -*- */
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

// Converts textual compressed algorithm into binary file form.

#include "sexp/FlattenAst.h"
#include "sexp-parser/Driver.h"

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::interp;

namespace {

const char* InputFilename = "-";
const char* OutputFilename = "-";

#if 0
std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    return std::make_shared<FdWriter>(STDOUT_FILENO, false);
  }
  return std::make_shared<FileWriter>(OutputFilename);
}
#endif

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options] FILE\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Convert CAST FILE to CASM binary.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr, "  -o File\t\tGenerated CASM binary ('-' implies stdout).\n");
  if (isDebug()) {
    fprintf(stderr,
            "  -v | --verbose\t"
            "Show progress (can be repeated for more detail).\n");
    fprintf(stderr,
            "\t\t\t-v       : Show progress of writing out CASM file.\n");
    fprintf(stderr, "\t\t\t-v -v    : Add tracing of parsing s-expressions.\n");
    fprintf(stderr, "\t\t\t-v -v -v : Add tracing of lexing s-expressions.\n");
  }
}


}  // end of anonymous namespace

int main(int Argc, char* Argv[]) {
  int Verbose = 0;
#if 0
  bool MinimizeBlockSize = false;
#endif
  bool InputSpecified = false;
  bool OutputSpecified = false;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail")) {
      ExpectExitFail = true;
    } else if (Argv[i] == std::string("-h") ||
               Argv[i] == std::string("--help")) {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
#if 0
    } else if (Argv[i] == std::string("-m")) {
      MinimizeBlockSize = true;
#endif
    } else if (Argv[i] == std::string("-o")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      if (OutputSpecified) {
        fprintf(stderr, "-i <output> option can't be repeated\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
      OutputSpecified = true;
    } else if (isDebug() && (Argv[i] == std::string("-v") ||
                             (Argv[i] == std::string("--verbose")))) {
      ++Verbose;
    } else if (!InputSpecified) {
      InputFilename = Argv[i];
      InputSpecified = true;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  auto Symtab = std::make_shared<SymbolTable>();
  Driver Parser(Symtab);
  if (Verbose >= 2) {
    Parser.setTraceParsing(Verbose >= 2);
    Parser.setTraceLexing(Verbose >= 3);
  }
  if (!Parser.parse(InputFilename)) {
    fprintf(stderr, "Unable to parse s-expressions: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }
  std::shared_ptr<IntStream> IntSeq = std::make_shared<IntStream>();
  FlattenAst Flattener(IntSeq, Symtab);
  Flattener.setTraceProgress(Verbose >= 1);
  if (!Flattener.flatten()) {
    fprintf(stderr, "Problems flattening CAST ast!\n");
    return exit_status(EXIT_FAILURE);
  }
#if 0
  std::shared_ptr<RawStream> Output = getOutput();
#endif
  fprintf(stderr,
          "Conversion from integer stream to byte stream not implemented!\n");
  return exit_status(EXIT_SUCCESS);
}
