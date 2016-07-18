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

#include "binary/BinaryWriter.h"
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

bool UseFileStreams = true;
const char *InputFilename = "-";
const char *OutputFilename = "-";

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

void usage(const char *AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Convert WASM filter s-expressions to WASM binary.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --expect-fail\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\tFile of s-expressions ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\tGenerated WASM binary ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\tUse C++ streams instead of C file descriptors.\n");
  fprintf(stderr, "  -t\t\tTrace progress decompressing.\n");
}

int main(int Argc, char *Argv[]) {
  bool TraceProgress = false;
  bool MinimizeBlockSize = false;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail")) {
      ExpectExitFail = true;
    } else if (Argv[i] == std::string("-h")
             || Argv[i] == std::string("--help")) {
      usage(Argv[i]);
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
        TraceProgress = true;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }

  }
  // TODO(karlschimpf) Use arena allocator once working.
  wasm::alloc::Malloc Allocator;
  SymbolTable SymTab(&Allocator);
  Driver Parser(SymTab);
  if (!Parser.parse(InputFilename)) {
    fprintf(stderr, "Unable to parse s-expressions: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }
  WriteBackedByteQueue Output(getOutput());
  SymbolTable Symtab(&Allocator);
  BinaryWriter Writer(&Output, Symtab);
  Writer.setTraceProgress(TraceProgress);
  Writer.setMinimizeBlockSize(MinimizeBlockSize);
  Writer.writePreamble();
  Writer.writeFile(wasm::dyn_cast<FileNode>(Parser.getParsedAst()));
  return exit_status(EXIT_SUCCESS);
}
