/* -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "algorithms/wasm0xd.h"
#include "binary/BinaryReader.h"
#include "intcomp/IntCompress.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/RawStream.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

#include "utils/Defs.h"

#include <cstring>
#include <unistd.h>

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::intcomp;
using namespace wasm::utils;

charstring InputFilename = "-";
charstring OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-")) {
    return std::make_shared<FdReader>(STDIN_FILENO, false);
  }
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    return std::make_shared<FileWriter>(stdout, false);
  }
  return std::make_shared<FileWriter>(OutputFilename);
}

int main(int Argc, const char* Argv[]) {
  charstring AlgorithmFilename = nullptr;
  bool Verbose = false;
  IntCompressor::Flags CompressionFlags;

  {
    ArgsParser Args("Compress integer sequences in a WASM file");

    ArgsParser::Required<charstring> InputFilenameFlag(InputFilename);
    Args.add(InputFilenameFlag
             .setOptionName("INPUT")
             .setDescription("WASM file to compress"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(OutputFilenameFlag
             .setShortName('o')
             .setLongName("output")
             .setOptionName("OUTPUT")
             .setDescription("Place to put resulting compressed WASM binary"));

    ArgsParser::Optional<charstring> AlgorithmFilenameFlag(AlgorithmFilename);
    Args.add(AlgorithmFilenameFlag
             .setShortName('a')
             .setLongName("algorithm")
             .setOptionName("ALGORITHM")
             .setDescription("File containing algorithm to parse WASM file "
                             "(rather than using builting algorithm)"));

    ArgsParser::Optional<size_t> CountCutoffFlag(CompressionFlags.CountCutoff);
    Args.add(CountCutoffFlag
             .setDefault(10)
             .setLongName("min-int-count")
             .setOptionName("INTEGER")
             .setDescription("Minimum number of times an integer must appear "
                             "to be considered for compression"));

    ArgsParser::Optional<size_t>
        WeightCutoffFlag(CompressionFlags.WeightCutoff);
    Args.add(WeightCutoffFlag
             .setDefault(100)
             .setLongName("min-weight")
             .setOptionName("INTEGER")
             .setDescription("Minimum number of integers (single and/or "
                             "sequences) that are needed to be considered a "
                             "compression pattern"));

    ArgsParser::Optional<size_t> LengthLimitFlag(CompressionFlags.LengthLimit);
    Args.add(LengthLimitFlag
             .setDefault(5)
             .setLongName("max-length")
             .setOptionName("INTEGER")
             .setDescription("Maximum integer sequence length that will be "
                             "considered for compression patterns ("
                             "execution time grows non-linearly when this value "
                             " is increased)"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(VerboseFlag
             .setShortName('v')
             .setLongName("verbose")
             .setDescription("Show progress of compression"));

    ArgsParser::Optional<bool>
        TraceReadingInputFlag(CompressionFlags.TraceReadingInput);
    Args.add(TraceReadingInputFlag
             .setLongName("verbose=read")
             .setDescription("Show trace of initial read of the WASM file"));

    ArgsParser::Optional<bool>
        TraceReadingIntStreamFlag(CompressionFlags.TraceReadingIntStream);
    Args.add(TraceReadingIntStreamFlag
             .setLongName("verbose=reread")
             .setDescription("Show trace of subsequent reads of the integer "
                             "stream produced by the intial read"));

    ArgsParser::Optional<bool>
        TraceWritingCodeOutputFlag(CompressionFlags.TraceWritingCodeOutput);
    Args.add(TraceWritingCodeOutputFlag
             .setLongName("verbose=code")
             .setDescription("Show trace of generated compression algorithm"));

    ArgsParser::Optional<bool>
        TraceCodeGenerationForReadingFlag(
            CompressionFlags.TraceCodeGenerationForReading);
    Args.add(TraceCodeGenerationForReadingFlag
             .setLongName("verbose=read-code")
             .setDescription("Show trace of generating code to compress the "
                             "integer stream produced by the initial read, to "
                             "the corresponding compressed integer stream"));

    ArgsParser::Optional<bool> TraceCodeGenerationForWritingFlag(
        CompressionFlags.TraceCodeGenerationForWriting);
    Args.add(TraceCodeGenerationForWritingFlag
             .setLongName("verbose=write-code")
             .setDescription("Show trace of generating code to write out the "
                             "generated compressed integer stream"));

    ArgsParser::Optional<bool>
        TraceWritingDataOutputFlag(CompressionFlags.TraceWritingDataOutput);
    Args.add(TraceWritingDataOutputFlag
             .setLongName("verbose=data")
             .setDescription("Show trace of how data is compressed in the "
                             "output file"));

    ArgsParser::Optional<bool>
        TraceCompressionFlag(CompressionFlags.TraceCompression);
    Args.add(TraceCompressionFlag
             .setLongName("verbose=compress")
             .setDescription("Show details on how patterns are detected for "
                             "compressing the (input) integer sequence"));

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

  auto Symtab = std::make_shared<SymbolTable>();

  // TODO(karlschimpf) Fill in code here to get default algorithm if not
  // explicitly defined.

  IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                           std::make_shared<WriteBackedQueue>(getOutput()),
                           Symtab, CompressionFlags);
  Compressor.compress(Verbose
                      ? IntCompressor::DetailLevel::SomeDetail
                      : IntCompressor::DetailLevel::NoDetail);
  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
