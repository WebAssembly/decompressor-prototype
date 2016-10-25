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
#include "stream/RawStream.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>

using namespace wasm;
using namespace wasm::decode;

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
  fprintf(stderr, "  -d File\t\tFile containing default algorithms.\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile to decompress ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\t\tGenerated Decompressed File ('-' implies stdout).\n");
  fprintf(stderr, "  -p\t\t\tDon't install predefined decompression rules.\n");
  if (isDebug()) {
    fprintf(stderr,
            "  -v | --verbose\t"
            "Show progress (can be repeated for more detail).\n");
    fprintf(stderr,
            "\t\t\t-v          : "
            "Trace progress of compression.\n");
    fprintf(stderr,
            "\t\t\t-v -v       : "
            "Add progress of parsing default files.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v    : "
            "Add progress of lexing default files.\n");
  }
}

int main(int Argc, char* Argv[]) {
  int Verbose = 0;
  bool MinimizeBlockSize = false;
  bool InstallPredefinedRules = true;
  std::vector<int> DefaultIndices;
  for (int i = 1; i < Argc; ++i) {
    std::string Arg(Argv[i]);
    if (Arg == "-d") {
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
  auto Symtab = std::make_shared<SymbolTable>();
  if (InstallPredefinedRules &&
      !SymbolTable::installPredefinedDefaults(Symtab, Verbose >= 2)) {
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
      Reader.getTrace().setTraceProgress(Verbose >= 2);
      if (Reader.readFile()) {
        continue;
      }
    }
    Driver Parser(Symtab);
    Parser.setTraceParsing(Verbose >= 2);
    Parser.setTraceLexing(Verbose >= 3);
    if (!Parser.parse(Argv[i])) {
      fprintf(stderr, "Unable to parse default algorithms: %s\n", Argv[i]);
      return exit_status(EXIT_FAILURE);
    }
    IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                             std::make_shared<WriteBackedQueue>(getOutput()),
                             Symtab);
    Compressor.setTraceProgress(Verbose >= 1);
    Compressor.setMinimizeBlockSize(MinimizeBlockSize);
    Compressor.compress();
    if (Decompressor.errorsFound()) {
      fatal("Failed to compress due to errors!");
      exit_status(EXIT_FAILURE);
    }
  }
  return exit_status(EXIT_SUCCESS);
}
