// -*- C++ -*- */
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

#ifndef DECOMPRESSOR_SRC_INTCOMP_COMPRESSIONFLAGS_H
#define DECOMPRESSOR_SRC_INTCOMP_COMPRESSIONFLAGS_H

#include "interp/IntFormats.h"
#include "interp/InterpreterFlags.h"
#include "utils/Defs.h"

namespace wasm {

namespace intcomp {

typedef uint32_t CollectionFlags;

enum class CollectionFlag : CollectionFlags {
  None = 0x0,
  TopLevel = 0x1,
  IntPaths = 0x2,
  All = 0x3
};

inline CollectionFlags makeFlags(CollectionFlag F) {
  return CollectionFlags(F);
}

inline bool hasFlag(CollectionFlag F, CollectionFlags Flags) {
  return makeFlags(F) & Flags;
}

charstring getName(CollectionFlags Flags);

struct CompressionFlags {
  size_t CountCutoff;
  size_t WeightCutoff;
  size_t PatternLengthLimit;
  size_t PatternLengthMultiplier;
  size_t MaxAbbreviations;
  decode::IntType SmallValueMax;
  size_t SmallValueCountCutoff;
  interp::IntTypeFormat AbbrevFormat;
  bool MinimizeCodeSize;
  bool UseHuffmanEncoding;
  bool TrimOverriddenPatterns;
  bool BitCompressOpcodes;
  bool ReassignAbbreviations;
  bool UseCismModel;
  interp::IntTypeFormat DefaultFormat;
  interp::IntTypeFormat LoopSizeFormat;

  interp::InterpreterFlags MyInterpFlags;

  bool TraceHuffmanAssignments;
  bool TraceReadingInput;
  bool TraceReadingIntStream;
  bool TraceWritingCodeOutput;
  bool TraceWritingDataOutput;
  bool TraceCompression;
  bool TraceIntStreamGeneration;
  bool TraceCodeGenerationForReading;
  bool TraceCodeGenerationForWriting;
  bool TraceInputIntStream;
  bool TraceIntCounts;
  bool TraceIntCountsCollection;
  bool TraceSequenceCounts;
  bool TraceSequenceCountsCollection;
  bool TraceInitialAbbreviationAssignments;
  bool TraceAbbreviationAssignments;
  bool TraceAbbreviationAssignmentsCollection;
  bool TraceAssigningAbbreviations;
  bool TraceCompressedIntOutput;
  bool TraceAbbrevSelectionSelect;
  bool TraceAbbrevSelectionCreate;
  bool TraceAbbrevSelectionDetail;
  size_t TraceAbbrevSelectionProgress;
  CompressionFlags();
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_COMPRESSIONFLAGS_H
