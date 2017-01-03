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

#include "algorithms/wasm0xd.h"
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
#include "utils/ArgsParse.h"

#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace wasm;
using namespace wasm::filt;
using namespace wasm::decode;
using namespace wasm::interp;
using namespace wasm::utils;

bool UseFileStreams = true;
const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (UseFileStreams)
    return std::make_shared<FileReader>(InputFilename);
  if (InputFilename == std::string("-"))
    return std::make_shared<decode::StreamReader>(std::cin);
  return std::make_shared<FstreamReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (UseFileStreams)
    return std::make_shared<FileWriter>(OutputFilename);
  if (OutputFilename == std::string("-"))
    return std::make_shared<decode::StreamWriter>(std::cout);
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

#if 0
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
#endif

int main(const int Argc, const char* Argv[]) {
  // TODO(karlschimpf) Add other default algorithms.
  bool Verbose = false;
  bool VerboseTrace = false;
  bool MinimizeBlockSize = false;
  bool InstallPredefinedRules = true;
  bool UseCApi = false;
  size_t NumTries = 1;
#if 1
  {
    ArgsParser Args("Decompress WASM binary file");

    ArgsParser::Optional<bool> UseCApiFlag(UseCApi);
    Args.add(UseCApiFlag.setLongName("c-api")
                 .setDescription("Use C API to decompress"));

    ArgsParser::Optional<bool> ExpectExitFailFlag(ExpectExitFail);
    Args.add(ExpectExitFailFlag.setLongName("expect-fail")
                 .setDescription(
                     "Negate the exit status. That is, when true, "
                     "Succeed on failure exit and fail on success"));

    ArgsParser::Required<charstring> InputFilenameFlag(InputFilename);
    Args.add(InputFilenameFlag.setOptionName("INPUT")
                 .setDescription("INPUT is the File to decompress"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(
        OutputFilenameFlag.setShortName('o')
            .setOptionName("OUTPUT")
            .setDescription("Puts the decompressed input into file OUTPUT"));

    ArgsParser::Toggle UseFileStreamsFlag(UseFileStreams);
    Args.add(
        UseFileStreamsFlag.setDefault(true).setShortName('s').setDescription(
            "Toggles to use file streams (when true) "
            "instead of C++ streams"));

    ArgsParser::Toggle MinimizeBlockSizeFlag(MinimizeBlockSize);
    Args.add(MinimizeBlockSizeFlag.setDefault(true)
                 .setShortName('m')
                 .setLongName("minimize")
                 .setDescription(
                     "Toggle minimizing decompressed size (rather than "
                     "conanical size)"));

    ArgsParser::Optional<size_t> NumTriesFlag(NumTries);
    Args.add(
        NumTriesFlag.setLongName("tries").setOptionName("N").setDescription(
            "Decompress N times (used to test performance "
            "when N!=1)"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(VerboseFlag.setShortName('v')
                 .setLongName("verbose")
                 .setDescription("Show progress of decompression"));

    ArgsParser::Optional<bool> VerboseTraceFlag(VerboseTrace);
    Args.add(VerboseTraceFlag.setLongName("verbose=trace")
                 .setDescription("Show trace of each pass in decompression"));

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }
  }
#else
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
#endif
  if (UseCApi) {
    if (!InstallPredefinedRules) {
      fprintf(stderr, "-p and --c-api options not allowed");
      return exit_status(EXIT_FAILURE);
    }
    if (NumTries != 1) {
      fprintf(stderr, "-t and --c-api options not allowed");
      return exit_status(EXIT_FAILURE);
    }
    if (MinimizeBlockSize)
      fprintf(stderr, "--c-api ignores -m option\n");
    return exit_status(runUsingCApi(Verbose >= 1));
  }
  std::shared_ptr<SymbolTable> Symtab;
  if (InstallPredefinedRules)
    Symtab = getAlgwasm0xdSymtab();
  else
    Symtab = std::make_shared<SymbolTable>();
#if 0
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
#endif
  bool Succeeded = true;  // until proven otherwise.
  for (size_t i = 0; i < NumTries; ++i) {
    if (Verbose)
      fprintf(stderr, "Opening input file: %s\n", InputFilename);
    std::shared_ptr<RawStream> Input = getInput();
    if (Input->hasErrors()) {
      fprintf(stderr, "Problems opening %s!\n", InputFilename);
      return exit_status(EXIT_SUCCESS);
    }
    if (Verbose)
      fprintf(stderr, "Opening output file: %s\n", OutputFilename);
    std::shared_ptr<RawStream> Output = getOutput();
    if (Output->hasErrors()) {
      fprintf(stderr, "Problems opening %s!\n", OutputFilename);
      return exit_status(EXIT_SUCCESS);
    }
    if (Verbose)
      fprintf(stderr, "Decompressing...\n");
    Interpreter Decompressor(std::make_shared<ReadBackedQueue>(Input),
                             std::make_shared<WriteBackedQueue>(Output),
                             Symtab);
    Decompressor.setMinimizeBlockSize(MinimizeBlockSize);
    if (VerboseTrace) {
      auto Trace = std::make_shared<TraceClass>("Decompress");
      Trace->setTraceProgress(true);
      Decompressor.setTrace(Trace);
    }
    Decompressor.decompress();
    if (Decompressor.errorsFound()) {
      fatal("Failed to decompress due to errors!");
      Succeeded = false;
    }
  }
  return exit_status(Succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
}
