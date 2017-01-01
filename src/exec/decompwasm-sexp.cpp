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
#include "sexp/TextWriter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/ReadCursor.h"
#include "stream/StreamReader.h"
#include "stream/StreamWriter.h"
#include "stream/WriteCursor.h"
#include "utils/Defs.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace wasm;
using namespace wasm::filt;
using namespace wasm::decode;

bool UseFileStreams = true;
const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    if (UseFileStreams)
      return std::make_shared<FileReader>(stdin, false);
    return std::make_shared<StreamReader>(std::cin);
  }
  if (UseFileStreams)
    return std::make_shared<FileReader>(InputFilename);
  return std::make_shared<FstreamReader>(InputFilename);
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
          "  -r N\t\t\tUse a stream runner to read input N chars at a time.\n");
  fprintf(stderr,
          "  -o File\t\tFile with found s-expressions ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  if (isDebug()) {
    fprintf(stderr, "  -v | --verbose\tShow progress of reading wasm file.\n");
  }
}

int main(int Argc, char* Argv[]) {
  int Verbose = 0;
  int RunnerCount = 0;
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
    } else if (Argv[i] == std::string("-r")) {
      if (++i >= Argc) {
        fprintf(stderr, "No N specified after -r option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      RunnerCount = atoi(Argv[i]);
      if (RunnerCount == 0) {
        fprintf(stderr, "-r N must be greater than zero\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
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
  auto RawInput = getInput();
  auto Input = RunnerCount ? std::make_shared<Queue>()
                           : std::make_shared<ReadBackedQueue>(RawInput);
  auto Symtab = std::make_shared<SymbolTable>();
  BinaryReader Reader(Input, Symtab);
  Reader.getTrace().setTraceProgress(Verbose >= 1);
  if (RunnerCount) {
    constexpr size_t BufSize = 1024;
    uint8_t Buffer[BufSize];
    if (BufSize < size_t(RunnerCount))
      fatal("Buffer size (-r) can't exceed" + std::to_string(BufSize));
    WriteCursor FillPos(StreamType::Byte, Input);
    Reader.startReadingFile();
    bool ReachedEof = false;
    while (!Reader.isFinished()) {
      if (!ReachedEof) {
        size_t Count = RawInput->read(Buffer, RunnerCount);
        for (size_t i = 0; i < Count; ++i)
          FillPos.writeByte(Buffer[i]);
        if (Count == 0) {
          FillPos.freezeEof();
          ReachedEof = true;
        }
      }
      Reader.resume();
    }
  } else {
    Reader.readFile();
  }
  FileNode* File = Reader.getFile();
  if (Reader.errorsFound()) {
    fprintf(stderr, "Unable to parse WASM module!\n");
    return exit_status(EXIT_FAILURE);
  }
  OutputHandler Output;
  TextWriter Writer;
  Writer.write(Output.getFile(), File);
  return exit_status(EXIT_SUCCESS);
}
