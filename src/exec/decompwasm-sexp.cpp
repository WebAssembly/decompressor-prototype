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

using namespace wasm;
using namespace wasm::alloc;
using namespace wasm::filt;
using namespace wasm::decode;

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

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Extract out filter s-expressions from WASM binary.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tWasm file to read ('-' implies stdin).\n");
  fprintf(stderr,
          "  -o File\t\tFile with found s-expressions ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  if (isDebug()) {
    fprintf(stderr, "  -v | --verbose\tShow progress of reading wasm file.\n");
  }
}

int main(int Argc, char* Argv[]) {
  int Verbose = 0;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail")) {
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
    } else if (Argv[i] == std::string("-o")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
    } else if (Argv[i] == std::string("-s")) {
      UseFileStreams = true;
    } else if (isDebug() && (Argv[i] == std::string("-v") ||
                             Argv[i] == std::string("--verbose"))) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  // TODO(karlschimpf) Use arena allocator once working.
  Malloc Allocator;
  ReadBackedByteQueue Input(getInput());
  SymbolTable Symtab(&Allocator);
  BinaryReader Reader(&Input, Symtab);
  Reader.setTraceProgress(Verbose >= 1);
  FileNode* File = Reader.readFile();
  if (File == nullptr) {
    fprintf(stderr, "Unable to parse WASM module!\n");
    return exit_status(EXIT_FAILURE);
  }
  TextWriter Writer;
  Writer.write(stdout, File);
  return exit_status(EXIT_FAILURE);
}
