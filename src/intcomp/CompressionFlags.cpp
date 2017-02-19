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
// See the License for the specific language governing permissions and
// limitations under the License.

#include "intcomp/CompressionFlags.h"

namespace wasm {

using namespace interp;

namespace intcomp {

namespace {

charstring CollectionFlagsName[] = {"None", "TopLevel", "IntPaths", "All"};

}  // end of anonymous namespace

charstring getName(CollectionFlags Flags) {
  if (Flags < size(CollectionFlagsName))
    return CollectionFlagsName[Flags];
  return "UnknownCollectionFlags";
}

CompressionFlags::CompressionFlags()
    : CountCutoff(0),
      WeightCutoff(0),
      PatternLengthLimit(10),
      PatternLengthMultiplier(2),
      MaxAbbreviations(4096),
      SmallValueMax(std::numeric_limits<uint8_t>::max()),
      SmallValueCountCutoff(2),
      AbbrevFormat(IntTypeFormat::Varuint64),
      MinimizeCodeSize(true),
      UseHuffmanEncoding(false),
      TrimOverriddenPatterns(false),
      ReassignAbbreviations(true),
      DefaultFormat(IntTypeFormat::Varint64),
      LoopSizeFormat(IntTypeFormat::Varuint64),
      TraceHuffmanAssignments(false),
      TraceReadingInput(false),
      TraceReadingIntStream(false),
      TraceWritingCodeOutput(false),
      TraceWritingDataOutput(false),
      TraceCompression(false),
      TraceIntStreamGeneration(false),
      TraceCodeGenerationForReading(false),
      TraceCodeGenerationForWriting(false),
      TraceInputIntStream(false),
      TraceIntCounts(false),
      TraceIntCountsCollection(false),
      TraceSequenceCounts(false),
      TraceSequenceCountsCollection(false),
      TraceInitialAbbreviationAssignments(false),
      TraceAbbreviationAssignments(false),
      TraceAbbreviationAssignmentsCollection(false),
      TraceAssigningAbbreviations(false),
      TraceCompressedIntOutput(false),
      TraceAbbrevSelectionSelect(false),
      TraceAbbrevSelectionCreate(false),
      TraceAbbrevSelectionDetail(false),
      TraceAbbrevSelectionProgress(0) {
}

}  // end of namespace intcomp

}  // end of namespace wasm
