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
#include "intcomp/IntCompress.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

#include "utils/Defs.h"

#include <cstdlib>
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
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  return std::make_shared<FileWriter>(OutputFilename);
}

int main(int Argc, const char* Argv[]) {
  charstring AlgorithmFilename = nullptr;
  IntCompressor::Flags CompressionFlags;

  {
    ArgsParser Args("Compress integer sequences in a WASM file");

    ArgsParser::Required<charstring> InputFilenameFlag(InputFilename);
    Args.add(InputFilenameFlag.setOptionName("INPUT")
                 .setDescription("WASM file to compress"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(
        OutputFilenameFlag.setShortName('o')
            .setLongName("output")
            .setOptionName("OUTPUT")
            .setDescription("Place to put resulting compressed WASM binary"));

    ArgsParser::Optional<charstring> AlgorithmFilenameFlag(AlgorithmFilename);
    Args.add(AlgorithmFilenameFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "File containing algorithm to parse WASM file "
                     "(rather than using builting algorithm)"));

    ArgsParser::Optional<bool> UseHuffmanEncodingFlag(
        CompressionFlags.UseHuffmanEncoding);
    Args.add(UseHuffmanEncodingFlag.setLongName("Huffman").setDescription(
        "Usage Huffman encoding for abbreviations instead "
        "of a simple weighted ordering (experimental)"));

    ArgsParser::Optional<bool> TraceHuffmanAssignmentsFlag(
        CompressionFlags.TraceHuffmanAssignments);
    Args.add(
        TraceHuffmanAssignmentsFlag.setLongName("verbose=Huffman-assignments")
            .setDescription(
                "Show defined Huffman encoding assignments for "
                "to use for abbreviations"));

    ArgsParser::Optional<uint64_t> CountCutoffFlag(
        CompressionFlags.CountCutoff);
    Args.add(CountCutoffFlag.setDefault(10)
                 .setLongName("min-int-count")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum number of times an integer must appear "
                     "to be considered for compression"));

    ArgsParser::Optional<uint64_t> WeightCutoffFlag(
        CompressionFlags.WeightCutoff);
    Args.add(WeightCutoffFlag.setDefault(100)
                 .setLongName("min-weight")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum number of integers (single and/or "
                     "sequences) that are needed to be considered a "
                     "compression pattern"));

    ArgsParser::Optional<size_t> LengthLimitFlag(CompressionFlags.LengthLimit);
    Args.add(LengthLimitFlag.setDefault(5)
                 .setLongName("max-length")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Maximum integer sequence length that will be "
                     "considered for compression patterns ("
                     "execution time grows non-linearly when this value "
                     " is increased)"));

    ArgsParser::Optional<size_t> MaxAbbreviationsFlag(
        CompressionFlags.MaxAbbreviations);
    Args.add(
        MaxAbbreviationsFlag.setDefault(8192)
            .setLongName("max-abbreviations")
            .setOptionName("INTEGER")
            .setDescription(
                "Maximum number of abbreviations allowed in compressed file"));

    ArgsParser::Toggle TrimOverriddenPatternsFlag(
        CompressionFlags.TrimOverriddenPatterns);
    Args.add(TrimOverriddenPatternsFlag.setLongName("trim").setDescription(
        "Remove patterns if already implied by previous patterns"));

    ArgsParser::Optional<bool> TraceReadingInputFlag(
        CompressionFlags.TraceReadingInput);
    Args.add(
        TraceReadingInputFlag.setLongName("verbose=read")
            .setDescription("Show trace of initial read of the WASM file"));

    ArgsParser::Optional<bool> TraceReadingIntStreamFlag(
        CompressionFlags.TraceReadingIntStream);
    Args.add(TraceReadingIntStreamFlag.setLongName("verbose=reread")
                 .setDescription(
                     "Show trace of subsequent reads of the integer "
                     "stream produced by the intial read"));

    ArgsParser::Optional<bool> TraceWritingCodeOutputFlag(
        CompressionFlags.TraceWritingCodeOutput);
    Args.add(
        TraceWritingCodeOutputFlag.setLongName("verbose=code")
            .setDescription("Show trace of generated compression algorithm"));

    ArgsParser::Optional<bool> TraceCodeGenerationForReadingFlag(
        CompressionFlags.TraceCodeGenerationForReading);
    Args.add(TraceCodeGenerationForReadingFlag.setLongName("verbose=read-code")
                 .setDescription(
                     "Show trace of generating code to compress the "
                     "integer stream produced by the initial read, to "
                     "the corresponding compressed integer stream"));

    ArgsParser::Optional<bool> TraceCodeGenerationForWritingFlag(
        CompressionFlags.TraceCodeGenerationForWriting);
    Args.add(TraceCodeGenerationForWritingFlag.setLongName("verbose=write-code")
                 .setDescription(
                     "Show trace of generating code to write out the "
                     "generated compressed integer stream"));

    ArgsParser::Optional<bool> TraceWritingDataOutputFlag(
        CompressionFlags.TraceWritingDataOutput);
    Args.add(TraceWritingDataOutputFlag.setLongName("verbose=data")
                 .setDescription(
                     "Show trace of how data is compressed in the "
                     "output file"));

    ArgsParser::Optional<bool> TraceCompressionFlag(
        CompressionFlags.TraceCompression);
    Args.add(TraceCompressionFlag.setShortName('v')
                 .setLongName("verbose")
                 .setDescription(
                     "Show basic details on how the input is comverted to "
                     "the corresponding compressed output"));

    ArgsParser::Optional<bool> TraceInputIntStreamFlag(
        CompressionFlags.TraceInputIntStream);
    Args.add(TraceInputIntStreamFlag.setLongName("verbose=int-input")
                 .setDescription("Show initial parsed integer stream"));

    ArgsParser::Optional<bool> TraceIntCountsFlag(
        CompressionFlags.TraceIntCounts);
    Args.add(
        TraceIntCountsFlag.setLongName("verbose=int-counts")
            .setDescription("Show frequency of integers in the input stream"));

    ArgsParser::Optional<bool> TraceIntCountsCollectionFlag(
        CompressionFlags.TraceIntCountsCollection);
    Args.add(TraceIntCountsCollectionFlag.setLongName(
                                              "verbose=int-counts-collection")
                 .setDescription("Show how int counts were selected"));

    ArgsParser::Optional<bool> TraceSequenceCountsFlag(
        CompressionFlags.TraceSequenceCounts);
    Args.add(TraceSequenceCountsFlag.setLongName("verbose=seq-counts")
                 .setDescription(
                     "Show frequency of integer sequences in the "
                     "input stream"));

    ArgsParser::Optional<bool> TraceSequenceCountsCollectionFlag(
        CompressionFlags.TraceSequenceCountsCollection);
    Args.add(
        TraceSequenceCountsCollectionFlag.setLongName(
                                              "verbose=seq-counts-collection")
            .setDescription(
                "Show how frequency of integer sequences were "
                "selected"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsFlag(
        CompressionFlags.TraceAbbreviationAssignments);
    Args.add(TraceAbbreviationAssignmentsFlag.setLongName("verbose=abbrev")
                 .setDescription("Show (initial) abbreviation assignments"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsCollectionFlag(
        CompressionFlags.TraceAbbreviationAssignmentsCollection);
    Args.add(TraceAbbreviationAssignmentsCollectionFlag
                 .setLongName("vebose=abbrev-collection")
                 .setDescription(
                     "Show how the (initial) abbreviation assignments "
                     "were selected"));

    ArgsParser::Optional<bool> TraceAssigningAbbreviationsFlag(
        CompressionFlags.TraceAssigningAbbreviations);
    Args.add(
        TraceAssigningAbbreviationsFlag.setLongName("verbose=assign-abbrevs")
            .setDescription("Show how abbreviations are assigned"));

    ArgsParser::Optional<bool> TraceCompressedIntOutputFlag(
        CompressionFlags.TraceCompressedIntOutput);
    Args.add(TraceCompressedIntOutputFlag.setLongName("verbose=int-output")
                 .setDescription("Show generated compressed integer stream"));

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

  // TODO(karlschimpf) Fill in code here to get default algorithm if not
  // explicitly defined.

  IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                           std::make_shared<WriteBackedQueue>(getOutput()),
                           getAlgwasm0xdSymtab(), CompressionFlags);
  Compressor.compress();

  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
