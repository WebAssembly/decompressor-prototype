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

// Tests reading/writing of files.

#include "stream/FileReader.h"
#include "stream/FileWriter.h"

#include <unistd.h>
#include <cstdlib>
#include <iostream>

using namespace wasm::decode;

namespace {

const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  return std::make_shared<FileWriter>(OutputFilename);
}

void usage(char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c N\t\tRead N bytes (i.e. chunksize) at a time\n");
  fprintf(stderr, "  --expect-fail\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\tShow usage\n");
  fprintf(stderr, "  -i NAME\tRead from input file NAME ('-' implies stdin)\n");
  fprintf(stderr,
          "  -o NAME\tWrite to output file NAME ('-' implies stdout)\n");
  fprintf(stderr, "  -s\t\tUse C++ streams instead of file descriptors\n");
}

}  // end of anonymous namespace

int main(int Argc, char* Argv[]) {
  int BufSize = 1;
  static constexpr int MaxBufSize = 4096;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail"))
      ExpectExitFail = true;
    else if (Argv[i] == std::string("-i")) {
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
    } else if (Argv[i] == std::string("-c")) {
      if (++i >= Argc) {
        fprintf(stderr, "No byte count after -c option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      int Size = atoi(Argv[i]);
      if (Size < 1) {
        fprintf(stderr, "Chunk size %d must be > 0\n", Size);
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      if (Size > MaxBufSize) {
        fprintf(stderr, "Chunk size %d can't exceed %d\n", Size, MaxBufSize);
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      BufSize = Size;
    } else if (Argv[i] == std::string("-h") ||
               (Argv[i] == std::string("--help"))) {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  std::shared_ptr<RawStream> Input = getInput();
  std::shared_ptr<RawStream> Output = getOutput();
  uint8_t Buffer[MaxBufSize];
  while (!Input->atEof()) {
    if (!Output->write(Buffer, Input->read(Buffer, BufSize))) {
      fprintf(stderr, "Write failed!\n");
      return exit_status(EXIT_FAILURE);
    }
  }
  return exit_status(EXIT_SUCCESS);
}
