//* -*- C++ -*- */
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
// See the License for the specific language governing permissions ando
// limitations under the License.

#include "algorithms/wasm0xd.h"
#include "casm/CasmReader.h"
#include "intcomp/IntCompress.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

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
  std::vector<charstring> AlgorithmFilenames;
  bool TraceAlgorithmRead;
  CompressionFlags MyCompressionFlags;

  {
    ArgsParser Args("Compress integer sequences in a WASM file");

    ArgsParser::Required<charstring> InputFilenameFlag(InputFilename);
    Args.add(InputFilenameFlag.setOptionName("INPUT").setDescription(
        "WASM file to compress"));

    ArgsParser::Optional<charstring> OutputFilenameFlag(OutputFilename);
    Args.add(
        OutputFilenameFlag.setShortName('o')
            .setLongName("output")
            .setOptionName("OUTPUT")
            .setDescription("Place to put resulting compressed WASM binary"));

    ArgsParser::OptionalVector<charstring> AlgorithmFilenamesFlag(
        AlgorithmFilenames);
    Args.add(AlgorithmFilenamesFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Instead of using the default wasm algorithm to parse "
                     "the WASM file, use the aglorithm defined by "
                     "ALGORITHM(s). If repeated, each file defines the "
                     "enclosing scope for the next ALGORITHM file"));

    ArgsParser::Toggle TraceAlgorithmReadFlag(TraceAlgorithmRead);
    Args.add(TraceAlgorithmReadFlag.setLongName("verbose=algorithm")
                 .setDescription("Trace reading ALGORITHM(s) files"));

    ArgsParser::Toggle UseHuffmanEncodingFlag(
        MyCompressionFlags.UseHuffmanEncoding);
    Args.add(UseHuffmanEncodingFlag.setLongName("Huffman").setDescription(
        "Toggles usage Huffman encoding for pattern abbreviations instead"
        "of a simple weighted ordering)"));

    ArgsParser::Toggle UseCismModelFlag(MyCompressionFlags.UseCismModel);
    Args.add(UseCismModelFlag.setLongName("cism").setDescription(
        "Generate compressed algorithm using Cism algorithm "
        "(experimental - not working yet)"));

    ArgsParser::Optional<bool> TraceHuffmanAssignmentsFlag(
        MyCompressionFlags.TraceHuffmanAssignments);
    Args.add(
        TraceHuffmanAssignmentsFlag.setDefault(true)
            .setLongName("verbose=Huffman-assignments")
            .setDescription("Show defined Huffman encoding assignments for "
                            "to use for pattern abbreviations"));

    ArgsParser::Optional<bool> TraceBitCompressOpcodesFlag(
        MyCompressionFlags.BitCompressOpcodes);
    Args.add(TraceBitCompressOpcodesFlag.setDefault(true)
                 .setLongName("bit-compress")
                 .setDescription(
                     "Perform bit compresssion on binary opcode expressions"));

    ArgsParser::Optional<size_t> CountCutoffFlag(
        MyCompressionFlags.CountCutoff);
    Args.add(CountCutoffFlag.setDefault(100)
                 .setLongName("min-count")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum number of uses of a (a non-small value) pattern "
                     "before it is considered for abbreviating"));

    ArgsParser::Optional<size_t> WeightCutoffFlag(
        MyCompressionFlags.WeightCutoff);
    Args.add(WeightCutoffFlag.setDefault(100)
                 .setLongName("min-weight")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Minimum weight of a pattern (i.e. number of uses times "
                     "number of integer constants in pattern) before it is "
                     "considered for abbreviating"));

    ArgsParser::Optional<size_t> PatternLengthLimitFlag(
        MyCompressionFlags.PatternLengthLimit);
    Args.add(
        PatternLengthLimitFlag.setDefault(5)
            .setLongName("max-length")
            .setOptionName("INTEGER")
            .setDescription("Maximum integer sequence length that will be "
                            "considered for compression patterns ("
                            "execution time grows non-linearly when this value "
                            " is increased)"));

    ArgsParser::Optional<size_t> PatternLengthMultiplierFlag(
        MyCompressionFlags.PatternLengthMultiplier);
    Args.add(PatternLengthMultiplierFlag.setLongName("window-multiplier")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "Multiplier of 'max-length' to get window size used to "
                     "figure out optimal layout of pattern abbreviations for "
                     "the window"));

    ArgsParser::Optional<size_t> MaxAbbreviationsFlag(
        MyCompressionFlags.MaxAbbreviations);
    Args.add(
        MaxAbbreviationsFlag.setLongName("max-patterns")
            .setOptionName("INTEGER")
            .setDescription(
                "Maximum number of abbreviations allowed in compressed file"));

    ArgsParser::Optional<IntType> SmallValueMaxFlag(
        MyCompressionFlags.SmallValueMax);
    Args.add(
        SmallValueMaxFlag.setLongName("max-small")
            .setOptionName("INTEGER")
            .setDescription(
                "Maximum value that should be considered a small value when "
                "applying small pattern abbreviations"));

    ArgsParser::Optional<size_t> SmallValueCountCutoffFlag(
        MyCompressionFlags.SmallValueCountCutoff);
    Args.add(
        SmallValueCountCutoffFlag.setDefault(5)
            .setLongName("small-min-count")
            .setOptionName("INTEGER")
            .setDescription("Mimimum number of uses of a small value before "
                            "it is considered an abbreviation pattern"));

    ArgsParser::Toggle TrimOverriddenPatternsFlag(
        MyCompressionFlags.TrimOverriddenPatterns);
    Args.add(TrimOverriddenPatternsFlag.setDefault(true)
                 .setLongName("trim")
                 .setDescription(
                     "Toggles removing patterns if already implied by previous "
                     "patterns"));

    ArgsParser::Optional<bool> TraceReadingInputFlag(
        MyCompressionFlags.TraceReadingInput);
    Args.add(
        TraceReadingInputFlag.setLongName("verbose=read")
            .setDescription("Show trace of initial read of the WASM file"));

    ArgsParser::Optional<bool> TraceReadingIntStreamFlag(
        MyCompressionFlags.TraceReadingIntStream);
    Args.add(
        TraceReadingIntStreamFlag.setLongName("verbose=reread")
            .setDescription("Show trace of subsequent reads of the integer "
                            "stream produced by the intial read"));

    ArgsParser::Optional<bool> TraceWritingCodeOutputFlag(
        MyCompressionFlags.TraceWritingCodeOutput);
    Args.add(
        TraceWritingCodeOutputFlag.setLongName("verbose=code")
            .setDescription("Show trace of generated compression algorithm"));

    ArgsParser::Optional<bool> TraceCodeGenerationForReadingFlag(
        MyCompressionFlags.TraceCodeGenerationForReading);
    Args.add(
        TraceCodeGenerationForReadingFlag.setLongName("verbose=read-code")
            .setDescription("Show trace of generating code to compress the "
                            "integer stream produced by the initial read, to "
                            "the corresponding compressed integer stream"));

    ArgsParser::Optional<bool> TraceCodeGenerationForWritingFlag(
        MyCompressionFlags.TraceCodeGenerationForWriting);
    Args.add(
        TraceCodeGenerationForWritingFlag.setLongName("verbose=write-code")
            .setDescription("Show trace of generating code to write out the "
                            "generated compressed integer stream"));

    ArgsParser::Optional<bool> TraceWritingDataOutputFlag(
        MyCompressionFlags.TraceWritingDataOutput);
    Args.add(TraceWritingDataOutputFlag.setLongName("verbose=data")
                 .setDescription("Show trace of how data is compressed in the "
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
    Args.add(TraceIntCountsCollectionFlag
                 .setLongName("verbose=int-counts-collection")
                 .setDescription("Show how int counts were selected"));

    ArgsParser::Optional<bool> TraceSequenceCountsFlag(
        MyCompressionFlags.TraceSequenceCounts);
    Args.add(TraceSequenceCountsFlag.setLongName("verbose=seq-counts")
                 .setDescription("Show frequency of integer sequences in the "
                                 "input stream"));

    ArgsParser::Optional<bool> TraceSequenceCountsCollectionFlag(
        MyCompressionFlags.TraceSequenceCountsCollection);
    Args.add(TraceSequenceCountsCollectionFlag
                 .setLongName("verbose=seq-counts-collection")
                 .setDescription("Show how frequency of integer sequences were "
                                 "selected"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsFlag(
        MyCompressionFlags.TraceAbbreviationAssignments);
    Args.add(
        TraceAbbreviationAssignmentsFlag.setLongName("verbose=abbreviations")
            .setDescription("Show (final) abbreviation assignments"));

    ArgsParser::Optional<bool> TraceInitialAbbreviationAssignmentsFlag(
        MyCompressionFlags.TraceInitialAbbreviationAssignments);
    Args.add(TraceInitialAbbreviationAssignmentsFlag
                 .setLongName("verbose=initial-abbreviations")
                 .setDescription(
                     "Show initial abbreviation assignments before selecting "
                     "patterns"));

    ArgsParser::Toggle ReassignAbbreviationsFlag(
        MyCompressionFlags.ReassignAbbreviations);
    Args.add(
        ReassignAbbreviationsFlag.setShortName('r')
            .setLongName("reassign")
            .setDescription("Toggle whether abbrevation are reassigned after "
                            "selecting patterns"));

    ArgsParser::Optional<bool> TraceAbbreviationAssignmentsCollectionFlag(
        MyCompressionFlags.TraceAbbreviationAssignmentsCollection);
    Args.add(
        TraceAbbreviationAssignmentsCollectionFlag
            .setLongName("vebose=abbrev-collection")
            .setDescription("Show how the (initial) abbreviation assignments "
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
    Args.add(
        TraceIntStreamGenerationFlag.setLongName("verbose=select-abbrevs")
            .setDescription("Trace the generation of the compressed integer "
                            "stream. and show how abbreviations are selected"));

    ArgsParser::Optional<size_t> TraceAbbrevSelectionProgressFlag(
        MyCompressionFlags.TraceAbbrevSelectionProgress);
    Args.add(TraceAbbrevSelectionProgressFlag
                 .setLongName("verbose=select-abbrevs-progress")
                 .setOptionName("INTEGER")
                 .setDescription(
                     "For every INTEGER values generated in the output integer "
                     "stream, generate a progress message. Use this to show "
                     "progress is being made, especially with large "
                     "command line overrides (INTEGER=0 turns off)"));

    ArgsParser::Optional<bool> TraceAbbrevSelectionSelectFlag(
        MyCompressionFlags.TraceAbbrevSelectionSelect);
    Args.add(
        TraceAbbrevSelectionSelectFlag
            .setLongName("verbose=select-abbrevs-select")
            .setDescription("Show selected pattern sequences, as they apply. "
                            "Only appiles when --verbose=select-abbrevs is "
                            "also true"));

    ArgsParser::Optional<bool> TraceAbbrevSelectionCreateFlag(
        MyCompressionFlags.TraceAbbrevSelectionCreate);
    Args.add(
        TraceAbbrevSelectionCreateFlag
            .setLongName("verbose=select-abbrevs-create")
            .setDescription(
                "Show each created pattern sequence that is tried (not just "
                "the selected ones). Only applies when "
                "--verbose=select-abbrevs is also true"));

    ArgsParser::Optional<bool> TraceAbbrevSelectionDetailFlag(
        MyCompressionFlags.TraceAbbrevSelectionDetail);
    Args.add(TraceAbbrevSelectionDetailFlag
                 .setLongName("verbose=select-abbrev-details")
                 .setDescription(
                     "Show additional detail (besides creating and selecting) "
                     "when creating the applied pattern sequence. Only applies "
                     "when --verbose=select-abbrevs is also true"));

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

  SymbolTable::SharedPtr AlgSymtab;
  if (AlgorithmFilenames.empty()) {
    if (MyCompressionFlags.TraceCompression)
      fprintf(stderr, "Using prebuilt casm algorithm\n");
    AlgSymtab = getAlgwasm0xdSymtab();
  }
  for (charstring Filename : AlgorithmFilenames) {
    if (MyCompressionFlags.TraceCompression)
      fprintf(stderr, "Reading: %s\n", Filename);
    CasmReader Reader;
    Reader.setInstall(true)
        .setTraceRead(TraceAlgorithmRead)
        .readTextOrBinary(Filename, AlgSymtab);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Problems reading: %s\n", Filename);
      return exit_status(EXIT_FAILURE);
    }
    AlgSymtab = Reader.getReadSymtab();
  }

  IntCompressor Compressor(std::make_shared<ReadBackedQueue>(getInput()),
                           std::make_shared<WriteBackedQueue>(getOutput()),
                           AlgSymtab, MyCompressionFlags);
  Compressor.compress();
  if (Compressor.errorsFound()) {
    fatal("Failed to compress due to errors!");
    exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
