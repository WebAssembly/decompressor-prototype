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
      return std::make_shared<FdReader>(STDIN_FILENO, false);
    return std::make_shared<StreamReader>(std::cin);
  }
  if (UseFileStreams)
    return std::make_shared<FileReader>(InputFilename);
  return std::make_shared<FstreamReader>(OutputFilename);
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
  FileNode* File = nullptr;
  auto Input = std::make_shared<ReadBackedQueue>(getInput());
  auto Symtab = std::make_shared<SymbolTable>();
  if (RunnerCount) {
    ReadCursor RawReadPos(StreamType::Byte, Input);
    auto FillQueue = std::make_shared<Queue>();
    auto FillReadPos =
        std::make_shared<ReadCursor>(StreamType::Byte, FillQueue);
    std::shared_ptr<BinaryReader::Runner> Runner =
        BinaryReader::startReadingFile(FillReadPos, Symtab);
    Runner->getTrace().setTraceProgress(Verbose >= 1);
    WriteCursor* FillPos = Runner->getFillPos().get();
    while (Runner->needsMoreInput()) {
      // TODO(karlschimpf) Get a cleaner API for this!
      // Fill queue with more input and resume.
      size_t BytesRemaining = RunnerCount;
      while (BytesRemaining) {
        if (RawReadPos.atEof()) {
          FillPos->freezeEof();
          break;
        }
        FillPos->writeByte(RawReadPos.readByte());
        --BytesRemaining;
      }
      Runner->resumeReading();
    }
    if (Runner->errorsFound())
      fatal("Errors found while reading filter section!");
    File = Runner->getFile();
  } else {
    BinaryReader Reader(Input, Symtab);
    Reader.getTrace().setTraceProgress(Verbose >= 1);
    File = Reader.readFile();
  }
  if (File == nullptr) {
    fprintf(stderr, "Unable to parse WASM module!\n");
    return exit_status(EXIT_FAILURE);
  }
  TextWriter Writer;
  Writer.write(stdout, File);
  return exit_status(EXIT_SUCCESS);
}
