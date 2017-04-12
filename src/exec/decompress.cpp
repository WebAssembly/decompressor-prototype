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

#include "algorithms/casm0x0.h"
#include "algorithms/cism0x0.h"
#include "algorithms/wasm0xd.h"
#include "interp/Decompress.h"
#include "interp/DecompressSelector.h"
#include "interp/ByteReader.h"
#include "interp/ByteWriter.h"
#include "interp/Interpreter.h"
#include "casm/CasmReader.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

namespace {

using namespace wasm;
using namespace wasm::filt;
using namespace wasm::decode;
using namespace wasm::interp;
using namespace wasm::utils;

const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  return std::make_shared<FileWriter>(OutputFilename);
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

std::vector<charstring> Algorithms;
std::vector<size_t> AlgorithmsSeparators;
size_t NextAlgorithm = 1;
size_t NextSeparator = 1;
size_t SeparatorIndex = 0;

void getNextSeparator() {
  fprintf(stderr, "get next %u %u\n", unsigned(SeparatorIndex),
          unsigned(NextSeparator));
  while (SeparatorIndex < AlgorithmsSeparators.size()) {
    size_t Separator = AlgorithmsSeparators[SeparatorIndex];
    ++SeparatorIndex;
    fprintf(stderr, "Sep = %u\n", unsigned(Separator));
    if (Separator >= NextSeparator) {
      NextSeparator = Separator;
      break;
    }
  }
}

}  // end of anonymous namespace

int main(const int Argc, const char* Argv[]) {
  bool Verbose = false;
  bool MinimizeBlockSize = false;
  bool UseCApi = false;
  size_t NumTries = 1;
  InterpreterFlags InterpFlags;

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

    ArgsParser::OptionalVector<charstring> AlgorithmsFlag(Algorithms);
    Args.add(AlgorithmsFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Parse ALGORITHM(s) and add before the set of known "
                     "algorithms."));

    ArgsParser::OptionalVectorSeparator<charstring> AlgorithmsSeparatorsFlag(
        AlgorithmsSeparators, Algorithms);
    Args.add(
        AlgorithmsSeparatorsFlag.setShortName('e')
            .setLongName("end")
            .setDescription(
                "Separator between algorithms. Only necessary if algorithms "
                "are defined using a sequence of enclosing algorithms. This "
                "marker denotes the end of the previous sequence of enclosing "
                "algorithms"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(
        OutputFilenameFlag.setShortName('o')
            .setOptionName("OUTPUT")
            .setDescription("Puts the decompressed input into file OUTPUT"));

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

    ArgsParser::Optional<bool> VerboseTraceFlag(InterpFlags.TraceProgress);
    Args.add(VerboseTraceFlag.setLongName("verbose=progress")
                 .setDescription("Show trace of each pass in decompression"));

    ArgsParser::Optional<bool> TraceIntermediateStreamsFlag(
        InterpFlags.TraceIntermediateStreams);
    Args.add(
        TraceIntermediateStreamsFlag.setLongName("verbose=intermediate")
            .setDescription(
                "Show contents of each stream between each applied algorithm"));

    ArgsParser::Optional<bool> TraceAppliedAlgorithmsFlag(
        InterpFlags.TraceAppliedAlgorithms);
    Args.add(
        TraceAppliedAlgorithmsFlag.setLongName("verbose=algorithms")
            .setDescription(
                "Show algorithms as they are applied to the compressed input"));

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

  if (UseCApi) {
    if (NumTries != 1) {
      fprintf(stderr, "-t and --c-api options not allowed");
      return exit_status(EXIT_FAILURE);
    }
    return exit_status(runUsingCApi(Verbose >= 1));
  }

  std::vector<std::shared_ptr<SymbolTable>> AdditionalAlgorithms;
  SymbolTable::SharedPtr AlgSymtab;
  getNextSeparator();
  for (size_t AlgIndex = 0; AlgIndex < Algorithms.size(); ++AlgIndex) {
    const char* Filename = Algorithms[AlgIndex];
    if (Verbose)
      fprintf(stderr, "Opening algorithm file (%" PRIuMAX "): %s\n",
              uintmax_t(NextAlgorithm), Filename);
    CasmReader Reader;
    Reader.setInstall(true).readTextOrBinary(Filename, AlgSymtab);
    AlgSymtab = Reader.getReadSymtab();
    size_t NextIndex = AlgIndex + 1;
    if (NextIndex == NextSeparator) {
      if (Verbose)
        fprintf(stderr, "Adding algorithm\n");
      AdditionalAlgorithms.push_back(AlgSymtab);
      ++NextAlgorithm;
      AlgSymtab.reset();
      ++NextSeparator;
      getNextSeparator();
    }
  }

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
    // Create input, output, and decompressor.
    std::shared_ptr<Queue> BackedOutput =
        std::make_shared<WriteBackedQueue>(Output);
    auto Writer = std::make_shared<ByteWriter>(BackedOutput);
    Interpreter Decompressor(
        std::make_shared<ByteReader>(std::make_shared<ReadBackedQueue>(Input)),
        Writer, InterpFlags);
    auto AlgState = std::make_shared<DecompAlgState>(&Decompressor);
    // Add additional algorithms first, so that they can override.
    for (std::shared_ptr<SymbolTable> Symtab : AdditionalAlgorithms) {
      Decompressor.addSelector(
          std::make_shared<DecompressSelector>(Symtab, AlgState));
    }
    // Add default predefined algorithsm.
    Decompressor.addSelector(
        std::make_shared<DecompressSelector>(getAlgcasm0x0Symtab(), AlgState));
    Decompressor.addSelector(
        std::make_shared<DecompressSelector>(getAlgwasm0xdSymtab(), AlgState));
    Decompressor.addSelector(
        std::make_shared<DecompressSelector>(getAlgcism0x0Symtab(), AlgState));
    // Decompress.
    Writer->setMinimizeBlockSize(MinimizeBlockSize);
    if (InterpFlags.TraceProgress) {
      auto Trace = std::make_shared<TraceClass>("Decompress");
      Trace->setTraceProgress(true);
      Decompressor.setTrace(Trace);
    }
    Decompressor.algorithmRead();
    if (Decompressor.errorsFound()) {
      fatal("Failed to decompress due to errors!");
      Succeeded = false;
    }
  }
  return exit_status(Succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
}
