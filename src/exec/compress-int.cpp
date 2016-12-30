/* -*- C++ -*- */
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

#include "algorithms/wasm0xd.h"
#include "binary/BinaryReader.h"
#include "intcomp/IntCompress.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/RawStream.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"
#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::intcomp;

charstring InputFilename = "-";
charstring OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    return std::make_shared<FdReader>(STDIN_FILENO, false);
  }
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    return std::make_shared<FdWriter>(STDOUT_FILENO, false);
  }
  return std::make_shared<FileWriter>(OutputFilename);
}

#if 0
void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Compress integer sequences in a WASM binary file.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "  -c N\t\t\tOnly compress sequences with count usage >= N.\n");
  fprintf(stderr, "  -d File\t\tFile containing default algorithms.\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile to decompress ('-' implies stdin).\n");
  fprintf(stderr, "  -l N\t\t\tOnly compress integer sequences <= N.\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\t\tGenerated Decompressed File ('-' implies stdout).\n");
  fprintf(stderr, "  -p\t\t\tDon't install predefined decompression rules.\n");
  fprintf(stderr,
          "  -w N\t\t\tOnly compress integer with weight usage >= N.\n");
  if (isDebug()) {
    fprintf(stderr,
            "  -v | --verbose\t"
            "Show progress (can be repeated for more detail).\n");
    fprintf(stderr,
            "\t\t\t-v\n"
            "\t\t\t\tAdd progress of compression.\n");
    fprintf(stderr,
            "\t\t\t-v -v\n"
            "\t\t\t\tAdd detailed progress of compression.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v\n"
            "\t\t\t\tAdd showing internal integer streams.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v\n"
            "\t\t\t\tAdd trace of initial parsing of (wasm) input file.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v -v\n"
            "\t\t\t\tAdd trace of subsequent parses of (wasm) input file.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v -v -v\n"
            "\t\t\t\tAdd progress of parsing default files.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v -v -v -v\n"
            "\t\t\t\tAdd progress of lexing default files.\n");
  }
}
#endif

int main(int Argc, char* Argv[]) {
  charstring AlgorithmFilename = nullptr;
  bool Verbose = false;
#if 0
  unsigned Verbose = 0;
  bool MinimizeBlockSize = false;
  bool InstallPredefinedRules = true;
  std::vector<int> DefaultIndices;
#endif
  IntCompressor::Flags CompressionFlags;

  {
    ArgsParser Args("Compress integer sequences in a WASM file");

    ArgsParser::Required<charstring> InputFilenameFlag(InputFilename);
    Args.add(InputFilenameFlag
             .setOptionName("INPUT")
             .setDescription("WASM file to compress"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(OutputFilename
             .setShortName('o')
             .setLongName("output")
             .setOptionName("OUTPUT")
             .setDescription("Place to put resulting compressed WASM binary"));

    ArgsParser::Optional<charstring> AlgorithmFilenameFlag(AlgorithmFile);
    Args.add(AlgorithmFilenameFlag
             .setShortName('a')
             .setLongName("algorithm")
             .setOptionName("ALGORITHM")
             .setDescription("File containing algorithm to parse WASM file "
                             "(rather than using builting algorithm)"));

    ArgsParser::Optional<size_t> CountCutoffFlag(CompressionFlags.CountCutoff);
    Args.add(CountCutoffFlag
             .setDefault(10)
             .setLongName('min-int-count')
             .setOptionName("INTEGER")
             .setDescription("Minimum number of times an integer must appear to "
                             "be considered for compression"));

    ArgsParser::Optional<size_t>
        WeightCutoffFlag(CompressionFlags.WeightCutoff);
    Args.add(WeightCutoffFlag
             .setDefault(100)
             .setLongName("min-weight")
             .setOptionName("INTEGER")
             .setDescription("Minimum number of integers (single and/or "
                             "sequences) that are needed to be considered a "
                             "compression pattern"));

    ArgsParser::Optional<size_t> LenghtLimitfFlag(CompressionFlags.LengthLimit);
    Args.add(LengthLimitFlag
             .setDefault(5)
             .setLongName("max-length")
             .setOptionName("INTEGER")
             .setDescription("Maximum integer sequence length that will be "
                             "considered for compression patterns ("
                             "execution time grows non-linearly when this value "
                             " is increased)"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(VerboseFlag
             .setShortName('v')
             .setLongName("verbose");
             .setDescription("Show progress of compression"));

    ArgsParser::Optional<bool> TracedReadingInputFlag(MyFlags.TraceReadingInput);
    Args.add(TraceReadingInputFlag
             .setLongName("verbose=read")
             .setDescription("Show trace of initial read of the WASM file"));

    ArgsParser::Optional<bool> TraceReadingIntStreamFlag(MyFlags.TraceReadingIntStream);
    Args.add(TraceReadingIntStreamFlag
             .setLongName("verbose=reread")
             .setDescription("Show trace of subsequent reads of the integer "
                             "stream produced by the intial read"));

    ArgsParser::Optional<bool> TraceWritingCodeOutputFlag(MyFlags.TracewritingCodeOutput);
    Args.add(TraceWritingCodeOutputFlag
             .setLongName("verbose=code")
             .setDescription("Show trace of generated compression algorithm"));

    ArgsParser::Optional<bool> TraceCodeGenerationForReadingFlag(
        MyFlags.TraceCodeGenerationForReading);
    Args.add(TraceCodeGenerationForReading
             .setLongName("verbose=read-code")
             .setDescription("Show trace of generating code to compress the "
                             "integer stream produced by the initial read, to "
                             "the corresponding compressed integer stream"));

    ArgsParser::Optional<bool> TraceCodeGenerationForWritingFlag(
        MyFlags.TraceCodeGenerationForWriting);
    Args.add(TraceCodeGenerationForWriting
             .setLongName("verbose=write-code")
             .setDescription("Show trace of generating code to write out the "
                             "generated compressed integer stream"));

    ArgsParser::Optional<bool> TraceWritingDataOutputFlag(MyFlags.TraceWritingDataOutput);
    Args.add(TraceWritingDataOutputFlag
             .setLongName("verbose=data")
             .setDescription("Show trace of how data is compressed in the "
                             "output file"));

    ArgsParser::Optional<bool> TraceCompressionFlag(MyFlags.TraceCompression);
    Args.add(TraceCompressionFlag
             .setLongName("verbose=compress")
             .setDescription("Show details on how patterns are detected for "
                             "compressing the (input) integer sequence"));

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
#if 0
  size_t CountCutoff = 10;
  size_t WeightCutoff = 100;
  size_t LengthLimit = 5;
  for (int i = 1; i < Argc; ++i) {
    std::string Arg(Argv[i]);
    if (Arg == "-c") {
      if (++i >= Argc) {
        fprintf(stderr, "No count N specified after -c option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      CountCutoff = atol(Argv[i]);
    } else if (Arg == "-w") {
      if (++i >= Argc) {
        fprintf(stderr, "No count N specified after -w option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      WeightCutoff = atol(Argv[i]);
    } else if (Arg == "-l") {
      if (++i >= Argc) {
        fprintf(stderr, "No count N specified after -l option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      LengthLimit = atol(Argv[i]);
    } else if (Arg == "-d") {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -d option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      DefaultIndices.push_back(i);
    } else if (Arg == "--expect-fail") {
      ExpectExitFail = true;
    } else if (Arg == "-h" || Arg == "--help") {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
    } else if (Arg == "-i") {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -i option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      InputFilename = Argv[i];
    } else if (Arg == "-m") {
      MinimizeBlockSize = true;
    } else if (Arg == "-o") {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
    } else if (Arg == "-p") {
      InstallPredefinedRules = false;
    } else if (isDebug() && (Arg == "-v" || Arg == "--verbose")) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }

  // Define defaults if only one cutoff provided.
  if (WeightCutoff == 0)
    WeightCutoff = CountCutoff;
  if (CountCutoff == 0)
    CountCutoff = WeightCutoff;

  // Extract compression detail level from verbose, and adjust, so
  // that remaining verbose checks need not be changed if detail level
  // changes.
  unsigned CompressVerbose = Verbose;
  constexpr unsigned MaxCompressVerbose = 3;
  if (Verbose < MaxCompressVerbose)
    Verbose = 0;
  else
    Verbose -= MaxCompressVerbose;

  auto Symtab = std::make_shared<SymbolTable>();
  if (InstallPredefinedRules &&
      !SymbolTable::installPredefinedDefaults(Symtab, Verbose >= 3)) {
    fprintf(stderr, "Unable to load compiled in default rules!\n");
    return exit_status(EXIT_FAILURE);
  }
  for (int i : DefaultIndices) {
    if (Verbose)
      fprintf(stderr, "Loading default: %s\n", Argv[i]);
    if (BinaryReader::isBinary(Argv[i])) {
      std::shared_ptr<RawStream> Stream = std::make_shared<FileReader>(Argv[i]);
      BinaryReader Reader(std::make_shared<ReadBackedQueue>(std::move(Stream)),
                          Symtab);
      Reader.getTrace().setTraceProgress(Verbose >= 3);
      if (Reader.readFile()) {
        continue;
      }
    }
    Driver Parser(Symtab);
    Parser.setTraceParsing(Verbose >= 3);
    Parser.setTraceLexing(Verbose >= 4);
    ;
    if (!Parser.parse(Argv[i])) {
      fprintf(stderr, "Unable to parse default algorithms: %s\n", Argv[i]);
      return exit_status(EXIT_FAILURE);
    }
  }

  IntCompressor::DetailLevel DetailLevel;
  switch (CompressVerbose) {
    case 0:
      DetailLevel = IntCompressor::NoDetail;
      break;
    case 1:
      DetailLevel = IntCompressor::SomeDetail;
      break;
    case 2:
      DetailLevel = IntCompressor::MoreDetail;
      break;
    default:
      DetailLevel = IntCompressor::AllDetail;
      break;
  }
#endif

  auto Symtab = std::make_shared<SymbolTable>();
  if (AlgorithmFilename == nullptr) {

  } else {
  }

  IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                           std::make_shared<WriteBackedQueue>(getOutput()),
                           Symtab);
  Compressor.setLengthLimit(LengthLimit);
  Compressor.setCountCutoff(CountCutoff);
  Compressor.setWeightCutoff(WeightCutoff);
  Compressor.setTraceProgress(CompressVerbose);
  Compressor.setMinimizeBlockSize(MinimizeBlockSize);
  Compressor.compress(DetailLevel, Verbose >= 1, Verbose <= 2);
  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
