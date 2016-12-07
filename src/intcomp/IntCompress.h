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

// Defines the compressor of WASM files based on integer usage.

#ifndef DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H
#define DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H

#include "intcomp/IntCountNode.h"
#include "intcomp/IntFormats.h"
#include "interp/IntStream.h"
#include "interp/StreamReader.h"
#include "interp/StreamWriter.h"
#include "sexp/TraceSexp.h"

namespace wasm {

namespace intcomp {

class IntCounterWriter;

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

class IntCompressor FINAL {
  IntCompressor() = delete;
  IntCompressor(const IntCompressor&) = delete;
  IntCompressor& operator=(const IntCompressor&) = delete;

 public:
  IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                std::shared_ptr<decode::Queue> OutputStream,
                std::shared_ptr<filt::SymbolTable> Symtab);

  ~IntCompressor();

  bool errorsFound() const { return ErrorsFound; }

  enum DetailLevel { NoDetail, SomeDetail, AllDetail };

  std::shared_ptr<RootCountNode> getRoot();

  void compress(DetailLevel Level = NoDetail,
                bool TraceParsing = false,
                bool TraceFirstPassOnly = true);

  void setTraceProgress(bool NewValue) {
    // TODO: Don't force creation of trace object if not needed.
    getTrace().setTraceProgress(NewValue);
  }
  bool getTraceProgress() {
    // TODO: Don't force creation of trace object if not needed.
    return getTrace().getTraceProgress();
  }
  void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace);
  filt::TraceClassSexp& getTrace() { return *getTracePtr(); }
  std::shared_ptr<filt::TraceClassSexp> getTracePtr();
  bool hasTrace() { return bool(Trace); }

  void setCountCutoff(uint64_t NewCutoff) { CountCutoff = NewCutoff; }
  void setWeightCutoff(uint64_t NewCutoff) { WeightCutoff = NewCutoff; }
  void setLengthLimit(size_t NewLimit) { LengthLimit = NewLimit; }

  void setMinimizeBlockSize(bool NewValue) { (void)NewValue; }

  IntTypeFormat getAbbrevFormat() const { return AbbrevFormat; }
  void setAbbrevFormat(IntTypeFormat Fmt) { AbbrevFormat = Fmt; }

  void describe(FILE* Out, CollectionFlags = makeFlags(CollectionFlag::All));

 private:
  std::shared_ptr<RootCountNode> Root;
  std::shared_ptr<decode::Queue> Input;
  std::shared_ptr<decode::Queue> Output;
  std::shared_ptr<filt::SymbolTable> Symtab;
  std::shared_ptr<interp::IntStream> Contents;
  std::shared_ptr<filt::TraceClassSexp> Trace;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  size_t LengthLimit;
  IntTypeFormat AbbrevFormat;
  bool ErrorsFound;
  void readInput(std::shared_ptr<decode::Queue> InputStream,
                 std::shared_ptr<filt::SymbolTable> Symtab,
                 bool TraceParsing);
  bool compressUpToSize(size_t Size, bool TraceParsing);
  void removeSmallUsageCounts();
  void assignInitialAbbreviations();
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H
