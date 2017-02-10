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
  CompressionFlags MyCompressionFlags;

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
        MyCompressionFlags.UseHuffmanEncoding);
    Args.add(UseHuffmanEncodingFlag.setLongName("Huffman").setDescription(
        "Usage Huffman encoding for abbreviations instead "
        "of a simple weighted ordering (experimental)"));

    ArgsParser::Optional<bool> TraceHuffmanAssignmentsFlag(
        MyCompressionFlags.TraceHuffmanAssignments);
    Args.add(
        TraceHuffmanAssignmentsFlag.setLongName("verbose=Huffman-assignments")
            .setDescription(
                "Show defined Huffman encoding assignments for "
                "to use for abbreviations"));

    ArgsParser::Optional<uint64_t> CountCutoffFlag(
        MyCompressionFlags.CountCutoff);
    Args.add(CountCutoffFlag.setDefault(10)
                 .setLongName("min-count")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum number of uses of a pattern before it is "
                     "considered for abbreviating"));

    ArgsParser::Optional<uint64_t> WeightCutoffFlag(
        MyCompressionFlags.WeightCutoff);
    Args.add(WeightCutoffFlag.setDefault(100)
                 .setLongName("min-weight")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum weight of a pattern (i.e. number of uses times "
                     "number of integer constants in pattern) before it is "
                     "considered for abbreviating"));

    ArgsParser::Optional<size_t> LengthLimitFlag(
        MyCompressionFlags.LengthLimit);
    Args.add(LengthLimitFlag.setDefault(5)
                 .setLongName("max-length")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Maximum integer sequence length that will be "
                     "considered for compression patterns ("
                     "execution time grows non-linearly when this value "
                     " is increased)"));

    ArgsParser::Optional<size_t> MaxAbbreviationsFlag(
        MyCompressionFlags.MaxAbbreviations);
    Args.add(
        MaxAbbreviationsFlag.setDefault(8192)
            .setLongName("max-abbreviations")
            .setOptionName("INTEGER")
            .setDescription(
                "Maximum number of abbreviations allowed in compressed file"));

    ArgsParser::Toggle TrimOverriddenPatternsFlag(
        MyCompressionFlags.TrimOverriddenPatterns);
    Args.add(TrimOverriddenPatternsFlag.setLongName("trim").setDescription(
        "Remove patterns if already implied by previous patterns"));

    ArgsParser::Toggle CheckOverlappingPatternsFlag(
        MyCompressionFlags.MyAbbrevAssignFlags.CheckOverlapping);
    Args.add(CheckOverlappingPatternsFlag.setLongName("overlapping")
                 .setDescription(
                     "Overlap close abbreviation patterns to find better "
                     "fit of abbreviations"));

    ArgsParser::Optional<bool> TraceReadingInputFlag(
        MyCompressionFlags.TraceReadingInput);
    Args.add(
        TraceReadingInputFlag.setLongName("verbose=read")
            .setDescription("Show trace of initial read of the WASM file"));

    ArgsParser::Optional<bool> TraceReadingIntStreamFlag(
        MyCompressionFlags.TraceReadingIntStream);
    Args.add(TraceReadingIntStreamFlag.setLongName("verbose=reread")
                 .setDescription(
                     "Show trace of subsequent reads of the integer "
                     "stream produced by the intial read"));

    ArgsParser::Optional<bool> TraceWritingCodeOutputFlag(
        MyCompressionFlags.TraceWritingCodeOutput);
    Args.add(
        TraceWritingCodeOutputFlag.setLongName("verbose=code")
            .setDescription("Show trace of generated compression algorithm"));

    ArgsParser::Optional<bool> TraceCodeGenerationForReadingFlag(
        MyCompressionFlags.TraceCodeGenerationForReading);
    Args.add(TraceCodeGenerationForReadingFlag.setLongName("verbose=read-code")
                 .setDescription(
                     "Show trace of generating code to compress the "
                     "integer stream produced by the initial read, to "
                     "the corresponding compressed integer stream"));

    ArgsParser::Optional<bool> TraceCodeGenerationForWritingFlag(
        MyCompressionFlags.TraceCodeGenerationForWriting);
    Args.add(TraceCodeGenerationForWritingFlag.setLongName("verbose=write-code")
                 .setDescription(
                     "Show trace of generating code to write out the "
                     "generated compressed integer stream"));

    ArgsParser::Optional<bool> TraceWritingDataOutputFlag(
        MyCompressionFlags.TraceWritingDataOutput);
    Args.add(TraceWritingDataOutputFlag.setLongName("verbose=data")
                 .setDescription(
                     "Show trace of how data is compressed in the "
                     "output file"));

    ArgsParser::Optional<bool> TraceCompressionFlag(
        MyCompressionFlags.TraceCompression);
    Args.add(TraceCompressionFlag.setShortName('v')
                 .setLongName("verbose")
                 .setDescription(
                     "Show basic details on how the input is comverted to "
                     "the corresponding compressed output"));

    ArgsParser::Optional<bool> TraceInputIntStreamFlag(
        MyCompressionFlags.TraceInputIntStream);
    Args.add(TraceInputIntStreamFlag.setLongName("verbose=int-input")
                 .setDescription("Show initial parsed integer stream"));

    ArgsParser::Optional<bool> TraceIntCountsFlag(
        MyCompressionFlags.TraceIntCounts);
    Args.add(
        TraceIntCountsFlag.setLongName("verbose=int-counts")
            .setDescription("Show frequency of integers in the input stream"));

    ArgsParser::Optional<bool> TraceIntCountsCollectionFlag(
        MyCompressionFlags.TraceIntCountsCollection);
    Args.add(TraceIntCountsCollectionFlag.setLongName(
                                              "verbose=int-counts-collection")
                 .setDescription("Show how int counts were selected"));

    ArgsParser::Optional<bool> TraceSequenceCountsFlag(
        MyCompressionFlags.TraceSequenceCounts);
    Args.add(TraceSequenceCountsFlag.setLongName("verbose=seq-counts")
                 .setDescription(
                     "Show frequency of integer sequences in the "
                     "input stream"));

    ArgsParser::Optional<bool> TraceSequenceCountsCollectionFlag(
        MyCompressionFlags.TraceSequenceCountsCollection);
    Args.add(
        TraceSequenceCountsCollectionFlag.setLongName(
                                              "verbose=seq-counts-collection")
            .setDescription(
                "Show how frequency of integer sequences were "
                "selected"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsFlag(
        MyCompressionFlags.TraceAbbreviationAssignments);
    Args.add(TraceAbbreviationAssignmentsFlag.setLongName("verbose=abbrev")
                 .setDescription("Show (initial) abbreviation assignments"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsCollectionFlag(
        MyCompressionFlags.TraceAbbreviationAssignmentsCollection);
    Args.add(TraceAbbreviationAssignmentsCollectionFlag
                 .setLongName("vebose=abbrev-collection")
                 .setDescription(
                     "Show how the (initial) abbreviation assignments "
                     "were selected"));

    ArgsParser::Optional<bool> TraceAssigningAbbreviationsFlag(
        MyCompressionFlags.TraceAssigningAbbreviations);
    Args.add(
        TraceAssigningAbbreviationsFlag.setLongName("verbose=assign-abbrevs")
            .setDescription("Show how abbreviations are assigned"));

    ArgsParser::Optional<bool> TraceCompressedIntOutputFlag(
        MyCompressionFlags.TraceCompressedIntOutput);
    Args.add(TraceCompressedIntOutputFlag.setLongName("verbose=int-output")
                 .setDescription("Show generated compressed integer stream"));

    ArgsParser::Optional<bool> TraceIntStreamGenerationFlag(
        MyCompressionFlags.TraceIntStreamGeneration);
    Args.add(TraceIntStreamGenerationFlag.setLongName("verbose=gen-int-output")
                 .setDescription(
                     "Trace the generation of the compressed integer "
                     "stream. In particular, show how abbreviations "
                     "are inserted"));

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
                           getAlgwasm0xdSymtab(), MyCompressionFlags);
  Compressor.compress();

  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
