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

#include "binary/BinaryReader.h"
#include "intcomp/IntCompress.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/RawStream.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::intcomp;

const char* InputFilename = "-";
const char* OutputFilename = "-";

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
            "\t\t\t-v                : "
            "Add progress of compression.\n");
    fprintf(stderr,
            "\t\t\t-v -v             : "
            "Add detailed progress of compression.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v          : "
            "Add trace of initial parsing of (wasm) input file..\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v       : "
            "Add trace of subsequent parses of (wasm) input file..\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v -v    : "
            "Add progress of parsing default files.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v -v -v : "
            "Add progress of lexing default files.\n");
  }
}

int main(int Argc, char* Argv[]) {
  unsigned Verbose = 0;
  bool MinimizeBlockSize = false;
  bool InstallPredefinedRules = true;
  std::vector<int> DefaultIndices;
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
  auto Symtab = std::make_shared<SymbolTable>();
  if (InstallPredefinedRules &&
      !SymbolTable::installPredefinedDefaults(Symtab, Verbose >= 5)) {
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
      Reader.getTrace().setTraceProgress(Verbose >= 5);
      if (Reader.readFile()) {
        continue;
      }
    }
    Driver Parser(Symtab);
    Parser.setTraceParsing(Verbose >= 5);
    Parser.setTraceLexing(Verbose >= 6);
    if (!Parser.parse(Argv[i])) {
      fprintf(stderr, "Unable to parse default algorithms: %s\n", Argv[i]);
      return exit_status(EXIT_FAILURE);
    }
  }
  IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                           std::make_shared<WriteBackedQueue>(getOutput()),
                           Symtab);
  Compressor.setLengthLimit(LengthLimit);
  Compressor.setCountCutoff(CountCutoff);
  Compressor.setWeightCutoff(WeightCutoff);
  Compressor.setTraceProgress(Verbose);
  Compressor.setMinimizeBlockSize(MinimizeBlockSize);
  IntCompressor::DetailLevel DetailLevel;
  switch (Verbose) {
    case 0:
      DetailLevel = IntCompressor::NoDetail;
      break;
    case 1:
      DetailLevel = IntCompressor::SomeDetail;
      break;
    default:
      DetailLevel = IntCompressor::AllDetail;
      break;
  }
  Compressor.compress(DetailLevel, Verbose >= 3, Verbose <= 3);
  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
