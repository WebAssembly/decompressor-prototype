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
#include "interp/Decompress.h"
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
    return std::make_shared<decode::StreamReader>(std::cin);
  }
  if (UseFileStreams)
    return std::make_shared<FileReader>(InputFilename);
  return std::make_shared<FstreamReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    if (UseFileStreams)
      return std::make_shared<FdWriter>(STDOUT_FILENO, false);
    return std::make_shared<decode::StreamWriter>(std::cout);
  }
  if (UseFileStreams)
    return std::make_shared<FileWriter>(OutputFilename);
  return std::make_shared<FstreamWriter>(OutputFilename);
}

int runUsingCApi(bool TraceProgress) {
  void* Decomp = create_decompressor();
  if (TraceProgress)
    set_trace_decompression(Decomp, TraceProgress);
  auto Input = getInput();
  auto Output = getOutput();
  constexpr int32_t MaxBufferSize = 4096;
  uint8_t* Buffer = get_decompressor_buffer(Decomp, MaxBufferSize);
  // Note: If Buffer size negative, it holds the final status of
  // the decompression.
  int32_t BufferSize = 0;
  bool MoreInput = true;
  while (BufferSize >= 0) {
    // Collect output if available.
    while (BufferSize > 0) {
      int32_t ChunkSize = std::min(BufferSize, MaxBufferSize);
      if (!fetch_decompressor_output(Decomp, ChunkSize)) {
        BufferSize = DECOMPRESSOR_ERROR;
        break;
      }
      if (!Output->write(Buffer, ChunkSize))
        BufferSize = DECOMPRESSOR_ERROR;
      BufferSize -= ChunkSize;
    }
    if (BufferSize < 0)
      break;
    // Fill the buffer with more input.
    while (MoreInput && BufferSize < MaxBufferSize) {
      size_t Count = Input->read(Buffer, MaxBufferSize - BufferSize);
      if (Count == 0) {
        MoreInput = false;
        break;
      }
      BufferSize += Count;
    }
    // Pass in new input and resume decompression.
    BufferSize = resume_decompression(Decomp, BufferSize);
  }
  int Result = BufferSize == DECOMPRESSOR_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
  return Result;
}

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Decompress WASM binary file.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --c-api\t\tUse C API to decompress.\n");
  fprintf(stderr, "  -d File\t\tFile containing default algorithms.\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile to decompress ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr,
          "  -o File\t\tGenerated Decompressed File ('-' implies stdout).\n");
  fprintf(stderr, "  -p\t\t\tDon't install predefined decompression rules.\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  fprintf(stderr,
          "  -t N\t\t\tDecompress N times (used to test performance).\n");
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
  int Verbose = 0;
  bool MinimizeBlockSize = false;
  bool InstallPredefinedRules = true;
  bool UseCApi = false;
  std::vector<int> DefaultIndices;
  size_t NumTries = 1;
  for (int i = 1; i < Argc; ++i) {
    std::string Arg(Argv[i]);
    if (Arg == "--c-api") {
      UseCApi = true;
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
    } else if (Arg == "-s") {
      UseFileStreams = true;
    } else if (Arg == "-t") {
      if (++i >= Argc) {
        fprintf(stderr, "No count N specified after -t option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      NumTries += atol(Argv[i]);
    } else if (isDebug() && (Arg == "-v" || Arg == "--verbose")) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }

  if (UseCApi) {
    if (!InstallPredefinedRules) {
      fprintf(stderr, "-p and --c-api options not allowed");
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
    if (!DefaultIndices.empty()) {
      fprintf(stderr, "-d and --c-api options not allowed");
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
    if (NumTries != 1) {
      fprintf(stderr, "-t and --c-api options not allowed");
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
    if (MinimizeBlockSize)
      fprintf(stderr, "--c-api ignores -m option\n");
    return exit_status(runUsingCApi(Verbose >= 1));
  }
  auto Symtab = std::make_shared<SymbolTable>();
  Symtab->getTrace().setTraceProgress(Verbose >= 4);
  if (InstallPredefinedRules &&
      !SymbolTable::installPredefinedDefaults(Symtab, Verbose >= 2)) {
    fprintf(stderr, "Unable to load compiled in default rules!\n");
    return exit_status(EXIT_FAILURE);
  }
  fprintf(stderr, "Reading default files...\n");
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
    fprintf(stderr, "Building decompressor...\n");
    Interpreter Decompressor(std::make_shared<ReadBackedQueue>(getInput()),
                             std::make_shared<WriteBackedQueue>(getOutput()),
                             Symtab);
    Decompressor.setTraceProgress(Verbose >= 1);
    Decompressor.setMinimizeBlockSize(MinimizeBlockSize);
    fprintf(stderr, "Decompressing...\n");
    Decompressor.decompress();
    if (Decompressor.errorsFound()) {
      fatal("Failed to decompress due to errors!");
      exit_status(EXIT_FAILURE);
    }
  }
  return exit_status(EXIT_SUCCESS);
}
