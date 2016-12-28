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

#include "interp/StreamWriter.h"
#include "interp/IntReader.h"
#include "interp/TeeWriter.h"
#include "sexp/FlattenAst.h"
#include "sexp/InflateAst.h"
#include "sexp-parser/Driver.h"
#include "stream/FileWriter.h"
#include "stream/WriteBackedQueue.h"
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
const char* AlgorithmFilename = "/dev/null";

bool TraceParser = false;
bool TraceLexer = false;

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    return std::make_shared<FdWriter>(STDOUT_FILENO, false);
  }
  return std::make_shared<FileWriter>(OutputFilename);
}

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

#if 0
void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options] FILE\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Convert CAST FILE to CASM binary.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "  -a FILE\t\tFile containing formatting algorithm for output\n");
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
#endif

}  // end of anonymous namespace

int main(int Argc, const char* Argv[]) {
  bool MinimizeBlockSize = false;
  bool Verbose = false;
  bool TraceFlattener = false;
  bool TraceWrite = false;
  bool TraceTree = false;
  {
    ArgsParser Args("Converts compression algorithm from text to binary");

    ArgsParser::OptionalCharstring AlgorithmFlag(AlgorithmFilename);
    Args.add(AlgorithmFlag
             .setShortName('a')
             .setOptionName("ALG")
             .setDescription("Use algorithm to parse text file"));

    ArgsParser::Bool ExpectFailFlag(ExpectExitFail);
    ExpectFailFlag
        .setDefault(false)
        .setLongName("expect-fail")
        .setDescription("Succeed on failure/fail on success");
    Args.add(ExpectFailFlag);

    ArgsParser::Bool MinimizeBlockFlag(MinimizeBlockSize);
    Args.add(MinimizeBlockFlag
             .setShortName('m')
             .setDescription("Minimize size in binary file "
                             "(note: runs slower)"));


    ArgsParser::RequiredCharstring InputFlag(InputFilename);
    Args.add(InputFlag
             .setOptionName("INPUT")
             .setDescription("Text file to convert to binary"));

    ArgsParser::OptionalCharstring OutputFlag(OutputFilename);
    Args.add(OutputFlag
             .setShortName('o')
             .setOptionName("OUTPUT")
             .setDescription("Generated binary file"));

    ArgsParser::Bool VerboseFlag(Verbose);
    Args.add(VerboseFlag
             .setShortName('v')
             .setLongName("verbose")
             .setDescription("Show progress of writing binary file"));

    ArgsParser::Bool TraceFlattenerFlag(TraceFlattener);
    Args.add(TraceFlattenerFlag
             .setLongName("trace=flattener")
             .setDescription("Trace how algorithms are flattened"));

    ArgsParser::Bool TraceWriteFlag(TraceWrite);
    Args.add(TraceWriteFlag
             .setLongName("trace=write")
             .setDescription("Trace writing of binary file"));

    ArgsParser::Bool TraceTreeFlag(TraceTree);
    Args.add(TraceTreeFlag
             .setLongName("trace=tree")
             .setDescription("Trace tree being written while writing "
                             "(implies --trace=write)"));

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }

    // Be sure to update implications!
    if (TraceTree)
      TraceWrite = true;
  }
  std::shared_ptr<SymbolTable> InputSymtab = parseFile(InputFilename);
  if (!InputSymtab)
    return exit_status(EXIT_FAILURE);
  std::shared_ptr<IntStream> IntSeq = std::make_shared<IntStream>();
  FlattenAst Flattener(IntSeq, InputSymtab);
  if (TraceFlattener) {
    auto Trace = std::make_shared<TraceClass>("flattener");
    Flattener.setTrace(Trace);
    Flattener.setTraceProgress(true);
  }
  if (!Flattener.flatten()) {
    fprintf(stderr, "Problems flattening CAST ast!\n");
    return exit_status(EXIT_FAILURE);
  }
  std::shared_ptr<SymbolTable> AlgSymtab = parseFile(AlgorithmFilename);
  if (!AlgSymtab)
    return exit_status(EXIT_FAILURE);
  std::shared_ptr<RawStream> Output = getOutput();
  if (Output->hasErrors()) {
    fprintf(stderr, "Problems opening output file: %s", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }
  auto BackedOutput = std::make_shared<WriteBackedQueue>(Output);
  std::shared_ptr<Writer> Writer = std::make_shared<StreamWriter>(BackedOutput);
  if (TraceTree) {
    auto Tee = std::make_shared<TeeWriter>();
    Tee->add(std::make_shared<InflateAst>(), false, true);
    Tee->add(Writer, true, false);
    Writer = Tee;
  }
  Writer->setMinimizeBlockSize(MinimizeBlockSize);
  auto Reader = std::make_shared<IntReader>(IntSeq, *Writer, AlgSymtab);
  if (TraceWrite) {
    auto Trace = std::make_shared<TraceClass>("write");
    Reader->setTrace(Trace);
    Writer->setTrace(Trace);
    Reader->setTraceProgress(true);
  }
  Reader->useFileHeader(InputSymtab->getInstalledFileHeader());
  Reader->algorithmStart();
  Reader->algorithmReadBackFilled();
  if (Reader->errorsFound()) {
    fprintf(stderr, "Problems while writing: %s\n", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
