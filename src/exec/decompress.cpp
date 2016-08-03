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
#include "stream/ByteQueue.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/StreamReader.h"
#include "stream/StreamWriter.h"
#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace wasm::filt;
using namespace wasm::decode;
using namespace wasm::interp;

bool UseFileStreams = true;
const char* InputFilename = "-";
const char* OutputFilename = "-";

std::unique_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    if (UseFileStreams)
      return FdReader::create(STDIN_FILENO, false);
    else
      return StreamReader::create(std::cin);
  }
  if (UseFileStreams)
    return FileReader::create(InputFilename);
  return FstreamReader::create(OutputFilename);
}

std::unique_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    if (UseFileStreams)
      return FdWriter::create(STDOUT_FILENO, false);
    else
      return StreamWriter::create(std::cout);
  }
  if (UseFileStreams)
    return FileWriter::create(OutputFilename);
  return FstreamWriter::create(OutputFilename);
}

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Decompress WASM binary file.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -d File\t\tFile containing default algorithms.\n");
  fprintf(stderr,
          "  --diff\t\tWhen tracing, "
          "show byte difference between reader/writer.\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile to decompress ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\t\tGenerated Decompressed File ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  fprintf(stderr,
          "  -v | --verbose\t"
          "Show progress (can be repeated for more detail).\n");
  fprintf(stderr, "\t\t\t-v          : "
          "Trace progress of decompression.\n");
  fprintf(stderr, "\t\t\t-v -v       : "
          "Add progress of parsing default files.\n");
  fprintf(stderr, "\t\t\t-v -v -v    : "
          "Add progress of lexing default files.\n");
  fprintf(stderr, "\t\t\t-v -v -v -v : "
          "Add progress of installing filter sections.\n");
}

int main(int Argc, char* Argv[]) {
  wasm::alloc::Malloc Allocator;
  SymbolTable SymTab(&Allocator);
  int Verbose = 0;
  bool TraceIoDifference = false;
  bool MinimizeBlockSize = false;
  std::vector<int> DefaultIndices;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("-d")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -d option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      DefaultIndices.push_back(i);
    } else if (Argv[i] == std::string("--diff")) {
      TraceIoDifference = true;
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
    } else if (Argv[i] == std::string("-v") ||
               Argv[i] == std::string("--verbose")) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  Node::Trace.setTraceProgress(Verbose >= 4);
  for (int i : DefaultIndices) {
    if (BinaryReader::isBinary(Argv[i])) {
      if (Verbose)
        fprintf(stderr, "Loading default file: %s", Argv[i]);
      std::unique_ptr<RawStream> Stream = FileReader::create(Argv[i]);
      ReadBackedByteQueue Input(std::move(Stream));
      BinaryReader Reader(&Input, SymTab);
      Reader.setTraceProgress(Verbose >= 2);
      if (FileNode* File = Reader.readFile()) {
        SymTab.install(File);
        continue;
      }
    }
    Driver Parser(SymTab);
    Parser.setTraceParsing(Verbose >= 2);
    Parser.setTraceLexing(Verbose >= 3);
    if (!Parser.parse(Argv[i])) {
      fprintf(stderr, "Unable to parse default algorithms: %s\n", Argv[i]);
      return exit_status(EXIT_FAILURE);
    }
  }
  ReadBackedByteQueue Input(getInput());
  WriteBackedByteQueue Output(getOutput());
  Interpreter Decompressor(&Input, &Output, &SymTab);
  Decompressor.setTraceProgress(Verbose >= 1, TraceIoDifference);
  Decompressor.setMinimizeBlockSize(MinimizeBlockSize);
  Decompressor.decompress();
  return exit_status(EXIT_SUCCESS);
}
