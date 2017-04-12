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

#include "algorithms/casm0x0.h"
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
  std::vector<charstring> AlgorithmFilenames;
  std::vector<charstring> InputFilenames;
  const char* OutputFilename = "-";
  bool InstallInput = true;
  bool Verbose = false;
  bool TraceParser = false;
  bool TraceLexer = false;
  bool TraceRead = false;
  bool TraceTree = false;

  {
    ArgsParser Args("Converts compression algorithm from binary fto text");
    ArgsParser::OptionalVector<charstring> AlgorithmFilenamesFlag(
        AlgorithmFilenames);
    Args.add(AlgorithmFilenamesFlag.setShortName('a')
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Instead of using the default casm algorithm to generate "
                     "the casm binary file, use the aglorithm defined by "
                     "ALGORITHM(s). If repeated, each file defines the "
                     "enclosing scope for the next ALGORITHM file"));

    ArgsParser::Optional<bool> ExpectFailFlag(ExpectExitFail);
    Args.add(ExpectFailFlag.setDefault(false)
                 .setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success"));

    ArgsParser::RequiredVector<charstring> InputFilenamesFlag(InputFilenames);
    Args.add(InputFilenamesFlag.setOptionName("INPUT").setDescription(
        "Binary file(s) to convert to text. If repeated, each file contains "
        "the enclosing algorithm for the next INPUT file."));

    ArgsParser::Optional<charstring> OutputFlag(OutputFilename);
    Args.add(OutputFlag.setShortName('o')
                 .setOptionName("OUTPUT")
                 .setDescription("Generated text file"));

    ArgsParser::Toggle InstallInputFlag(InstallInput);
    Args.add(InstallInputFlag.setLongName("install").setDescription(
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
  }

  SymbolTable::SharedPtr AlgSymtab;
  if (AlgorithmFilenames.empty()) {
    if (Verbose)
      fprintf(stderr, "Using prebuilt casm algorithm\n");
    AlgSymtab = getAlgcasm0x0Symtab();
  }
  for (charstring Filename : AlgorithmFilenames) {
    if (Verbose)
      fprintf(stderr, "Reading: %s\n", Filename);
    CasmReader Reader;
    Reader.setInstall(true)
        .setTraceRead(TraceRead)
        .setTraceTree(TraceTree)
        .readTextOrBinary(Filename, AlgSymtab);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Problems reading: %s\n", Filename);
      return exit_status(EXIT_FAILURE);
    }
    AlgSymtab = Reader.getReadSymtab();
  }

  if (InputFilenames.empty())
    InputFilenames.push_back("-");

  std::shared_ptr<SymbolTable> InputSymtab;
  for (charstring Filename : InputFilenames) {
    if (Verbose)
      fprintf(stderr, "Reading input: %s\n", Filename);
    CasmReader Reader;
    Reader.setInstall(InstallInput)
        .setTraceRead(TraceRead)
        .setTraceTree(TraceTree)
        .readTextOrBinary(Filename, InputSymtab, AlgSymtab);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Problems reading: %s\n", Filename);
      return exit_status(EXIT_FAILURE);
    }
    InputSymtab = Reader.getReadSymtab();
  }

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Writing file: %s\n", OutputFilename);
  OutputHandler Output(OutputFilename);
  InputSymtab->describe(Output.getFile());
  return exit_status(EXIT_SUCCESS);
}
