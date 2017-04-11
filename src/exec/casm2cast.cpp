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

// Converts binary algorithm file back to textual form.

#if WASM_BOOT == 0
#include "algorithms/casm0x0.h"
#endif
#include "casm/CasmReader.h"
#include "utils/ArgsParse.h"

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
// using namespace wasm::interp;
using namespace wasm::utils;

namespace {

class OutputHandler {
  OutputHandler(const OutputHandler&) = delete;
  OutputHandler& operator=(const OutputHandler&) = delete;

 public:
  OutputHandler(charstring Filename) : Out(stdout) {
    if (strcmp("-", Filename) == 0)
      return;
    Out = fopen(Filename, "w");
    if (Out == nullptr) {
      fprintf(stderr, "Unable to open: %s\n", Filename);
      exit(exit_status(EXIT_FAILURE));
    }
  }
  ~OutputHandler() {
    if (Out != stdout)
      fclose(Out);
  }
  FILE* getFile() { return Out; }

 private:
  FILE* Out;
};

}  // end of anonymous namespace

int main(int Argc, const char* Argv[]) {
  const char* InputFilename = "-";
  const char* OutputFilename = "-";
  const char* AlgorithmFilename = nullptr;
  bool InstallInput = true;
  bool Verbose = false;
  bool TraceParser = false;
  bool TraceLexer = false;
  bool TraceRead = false;
  bool TraceTree = false;

  {
    ArgsParser Args("Converts compression algorithm from binary fto text");

#if WASM_BOOT
    ArgsParser::Required<charstring>
#else
    ArgsParser::Optional<charstring>
#endif
        AlgorithmFlag(AlgorithmFilename);
    Args.add(
        AlgorithmFlag.setShortName('a').setOptionName("ALG").setDescription(
            "File containing algorithm defining binary "
            "format"));

    ArgsParser::Optional<bool> ExpectFailFlag(ExpectExitFail);
    Args.add(ExpectFailFlag.setDefault(false)
                 .setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success"));

    ArgsParser::Required<charstring> InputFlag(InputFilename);
    Args.add(InputFlag.setOptionName("INPUT")
                 .setDescription("BInary file to convert to text"));

    ArgsParser::Optional<charstring> OutputFlag(OutputFilename);
    Args.add(OutputFlag.setShortName('o')
                 .setOptionName("OUTPUT")
                 .setDescription("Generated text file"));

    ArgsParser::Toggle InstallInputFlag(InstallInput);
    Args.add(InstallInputFlag.setLongName("install")
             .setDescription(
                 "Install (i.e. validate) the read algorithm. Turn off "
                 "when reading an input file that needs an enclosing "
                 "algorithm to validate."));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(
        VerboseFlag.setShortName('v').setLongName("verbose").setDescription(
            "Show progress of conversion from binary to text"));

    ArgsParser::Optional<bool> TraceReadFlag(TraceRead);
    Args.add(
        TraceReadFlag.setLongName("verbose=read")
            .setDescription("Show how tree is constructed from binary file"));

    ArgsParser::Optional<bool> TraceTreeFlag(TraceTree);
    Args.add(TraceTreeFlag.setLongName("verbose=tree")
                 .setDescription(
                     "Show tree being built while reading"
                     "(implies --verbose=read)"));

    ArgsParser::Optional<bool> TraceParserFlag(TraceParser);
    Args.add(TraceParserFlag.setLongName("verbose=parser")
                 .setDescription(
                     "Show parsing of algorithm (defined by option -a)"));

    ArgsParser::Optional<bool> TraceLexerFlag(TraceLexer);
    Args.add(
        TraceLexerFlag.setLongName("verbose=lexer")
            .setDescription("Show lexing of algorithm (defined by option -a)"));

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }

    if (TraceTree)
      TraceRead = true;

    assert(!(WASM_BOOT && AlgorithmFilename == nullptr));
  }

  if (Verbose) {
    if (AlgorithmFilename)
      fprintf(stderr, "Reading algorithms file: %s\n", AlgorithmFilename);
    else
      fprintf(stderr, "Using prebuilt casm algorithm\n");
  }
  std::shared_ptr<SymbolTable> AlgSymtab;
  if (AlgorithmFilename) {
    CasmReader Reader;
    Reader.setTraceRead(TraceParser)
        .setTraceLexer(TraceLexer)
        .setTraceTree(TraceParser)
        .readText(AlgorithmFilename);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Problems reading file: %s\n", AlgorithmFilename);
      return exit_status(EXIT_FAILURE);
    }
    AlgSymtab = Reader.getReadSymtab();
#if WASM_BOOT == 0
  } else {
    AlgSymtab = getAlgcasm0x0Symtab();
#endif
  }

  if (Verbose)
    fprintf(stderr, "Reading input: %s\n", InputFilename);

  CasmReader Reader;
  Reader.setInstall(InstallInput).setTraceRead(TraceRead).setTraceTree(TraceTree).readBinary(
      InputFilename, AlgSymtab);

  if (Reader.hasErrors()) {
    fprintf(stderr, "Problems reading: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Writing file: %s\n", OutputFilename);
  OutputHandler Output(OutputFilename);
  Reader.getReadSymtab()->describe(Output.getFile());
  return exit_status(EXIT_SUCCESS);
}
