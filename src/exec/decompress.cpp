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

#include "binary/BinaryReader.h"
#include "interp/Interpreter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/StreamReader.h"
#include "stream/StreamWriter.h"
#include "stream/WriteBackedQueue.h"
#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace wasm;
using namespace wasm::filt;
using namespace wasm::decode;
using namespace wasm::interp;

bool UseFileStreams = true;
const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    if (UseFileStreams)
      return std::make_shared<FdReader>(STDIN_FILENO, false);
    return std::make_shared<StreamReader>(std::cin);
  }
  if (UseFileStreams)
    return std::make_shared<FileReader>(InputFilename);
  return std::make_shared<FstreamReader>(OutputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    if (UseFileStreams)
      return std::make_shared<FdWriter>(STDOUT_FILENO, false);
    return std::make_shared<StreamWriter>(std::cout);
  }
  if (UseFileStreams)
    return std::make_shared<FileWriter>(OutputFilename);
  return std::make_shared<FstreamWriter>(OutputFilename);
}

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Decompress WASM binary file.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -d File\t\tFile containing default algorithms.\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile to decompress ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\t\tGenerated Decompressed File ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  fprintf(stderr, "  -t N\t\t\tDecompress N times (used to test performance).\n");
  if (isDebug()) {
    fprintf(stderr,
            "  -v | --verbose\t"
            "Show progress (can be repeated for more detail).\n");
    fprintf(stderr,
            "\t\t\t-v          : "
            "Trace progress of decompression.\n");
    fprintf(stderr,
            "\t\t\t-v -v       : "
            "Add progress of parsing default files.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v    : "
            "Add progress of lexing default files.\n");
    fprintf(stderr,
            "\t\t\t-v -v -v -v : "
            "Add progress of installing filter sections.\n");
  }
}

int main(int Argc, char* Argv[]) {
  auto Symtab = std::make_shared<SymbolTable>();
  int Verbose = 0;
  bool MinimizeBlockSize = false;
  std::vector<int> DefaultIndices;
  size_t NumTries = 1;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("-d")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -d option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      DefaultIndices.push_back(i);
    } else if (Argv[i] == std::string("--expect-fail")) {
      ExpectExitFail = true;
    } else if (Argv[i] == std::string("-h") ||
               Argv[i] == std::string("--help")) {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
    } else if (Argv[i] == std::string("-i")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -i option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      InputFilename = Argv[i];
    } else if (Argv[i] == std::string("-m")) {
      MinimizeBlockSize = true;
    } else if (Argv[i] == std::string("-o")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
    } else if (Argv[i] == std::string("-s")) {
      UseFileStreams = true;
    } else if (Argv[i] == std::string("-t")) {
      if (++i >= Argc) {
        fprintf(stderr, "No count specified after -t option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      NumTries += atol(Argv[i]);
    } else if (isDebug() && (Argv[i] == std::string("-v") ||
                             Argv[i] == std::string("--verbose"))) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  Symtab->getTrace().setTraceProgress(Verbose >= 4);
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
  }
  for (size_t i = 0; i < NumTries; ++i) {
    Interpreter Decompressor(std::make_shared<ReadBackedQueue>(getInput()),
                             std::make_shared<WriteBackedQueue>(getOutput()),
                             Symtab);
    Decompressor.setTraceProgress(Verbose >= 1);
    Decompressor.setMinimizeBlockSize(MinimizeBlockSize);
    Decompressor.decompress();
    if (Decompressor.errorsFound()) {
      fatal("Failed to decompress due to errors!");
      exit_status(EXIT_FAILURE);
    }
  }
  return exit_status(EXIT_SUCCESS);
}
