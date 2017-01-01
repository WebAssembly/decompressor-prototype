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

#include "utils/Defs.h"

#if WASM_BOOT == 0
#include "algorithms/casm0x0.h"
#endif

#include "interp/StreamReader.h"
#include "sexp/InflateAst.h"
#include "sexp/TextWriter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/RawStream.h"
#include "stream/ReadBackedQueue.h"
#include "utils/ArgsParse.h"

#include <memory>

#include <unistd.h>

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::interp;
using namespace wasm::utils;

namespace {

const char* InputFilename = "-";
const char* OutputFilename = "-";
const char* AlgorithmFilename = nullptr;

bool TraceParser = false;
bool TraceLexer = false;

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    return std::make_shared<FileReader>(stdin, false);
  }
  return std::make_shared<FileReader>(InputFilename);
}

class OutputHandler {
  OutputHandler(const OutputHandler&) = delete;
  OutputHandler& operator=(const OutputHandler&) = delete;

 public:
  OutputHandler() : Out(stdout) {
    if (strcmp("-", OutputFilename) == 0)
      return;
    Out = fopen(OutputFilename, "w");
    if (Out == nullptr) {
      fprintf(stderr, "Unable to open: %s\n", OutputFilename);
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

std::shared_ptr<SymbolTable> parseFile(const char* Filename) {
  auto Symtab = std::make_shared<SymbolTable>();
  Driver Parser(Symtab);
  if (TraceParser)
    Parser.setTraceParsing(true);
  if (TraceLexer)
    Parser.setTraceLexing(true);
  if (!Parser.parse(Filename)) {
    fprintf(stderr, "Unable to parse: %s\n", Filename);
    Symtab.reset();
  }
  return Symtab;
}

}  // end of anonymous namespace

int main(int Argc, const char* Argv[]) {
  bool Verbose = false;
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

  assert(!(WASM_BOOT && AlgorithmFilename == nullptr));

  std::shared_ptr<SymbolTable> AlgSymtab;
  if (AlgorithmFilename) {
    if (Verbose)
      fprintf(stderr, "Reading algorithm file: %s\n", AlgorithmFilename);
    AlgSymtab = parseFile(AlgorithmFilename);
    if (!AlgSymtab)
      return exit_status(EXIT_FAILURE);
  } else {
    AlgSymtab = std::make_shared<SymbolTable>();
    install_Algcasm0x0(AlgSymtab);
  }

  if (Verbose)
    fprintf(stderr, "Reading input: %s\n", InputFilename);

  auto RawInput = getInput();
  auto Input = std::make_shared<ReadBackedQueue>(RawInput);
  InflateAst Inflator;
  auto StrmReader = std::make_shared<StreamReader>(Input, Inflator, AlgSymtab);
  if (TraceRead) {
    auto Trace = std::make_shared<TraceClass>("read");
    Trace->setTraceProgress(true);
    StrmReader->setTrace(Trace);
    if (TraceTree)
      Inflator.setTrace(Trace);
  }
  StrmReader->algorithmStart();
  StrmReader->algorithmReadBackFilled();
  if (StrmReader->errorsFound()) {
    fprintf(stderr, "Problems while reading: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Writing file: %s\n", OutputFilename);

  OutputHandler Output;
  TextWriter Writer;
  Writer.write(Output.getFile(), Inflator.getGeneratedFile());
  return exit_status(EXIT_SUCCESS);
}
