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

#include "intcomp/CompressionFlags.h"
#include "intcomp/IntCountNode.h"
#include "interp/IntFormats.h"
#include "interp/IntStream.h"
#include "interp/StreamReader.h"
#include "interp/StreamWriter.h"

namespace wasm {

namespace intcomp {

class IntCounterWriter;

class IntCompressor FINAL {
  IntCompressor() = delete;
  IntCompressor(const IntCompressor&) = delete;
  IntCompressor& operator=(const IntCompressor&) = delete;

 public:
  struct Flags {
    uint64_t CountCutoff;
    uint64_t WeightCutoff;
    size_t LengthLimit;
    interp::IntTypeFormat AbbrevFormat;
    bool MinimizeCodeSize;
    bool TraceReadingInput;
    bool TraceReadingIntStream;
    bool TraceWritingCodeOutput;
    bool TraceWritingDataOutput;
    bool TraceCompression;
    bool TraceIntStreamGeneration;
    bool TraceCodeGenerationForReading;
    bool TraceCodeGenerationForWriting;
    std::shared_ptr<utils::TraceClass> Trace;
    Flags();
  };

  IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                std::shared_ptr<decode::Queue> OutputStream,
                std::shared_ptr<filt::SymbolTable> Symtab,
                Flags& MyFlags);

  ~IntCompressor();

  bool errorsFound() const { return ErrorsFound; }

  enum DetailLevel { NoDetail, SomeDetail, MoreDetail, AllDetail };

  std::shared_ptr<RootCountNode> getRoot();

  void compress(DetailLevel Level = NoDetail);

  void setTraceProgress(bool NewValue) {
    // TODO: Don't force creation of trace object if not needed.
    getTrace().setTraceProgress(NewValue);
  }
  bool getTraceProgress() {
    // TODO: Don't force creation of trace object if not needed.
    return getTrace().getTraceProgress();
  }
  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  utils::TraceClass& getTrace() { return *getTracePtr(); }
  std::shared_ptr<utils::TraceClass> getTracePtr();
  bool hasTrace() { return bool(MyFlags.Trace); }

  void describe(FILE* Out, CollectionFlags = makeFlags(CollectionFlag::All));

 private:
  std::shared_ptr<RootCountNode> Root;
  std::shared_ptr<decode::Queue> Input;
  std::shared_ptr<decode::Queue> Output;
  Flags& MyFlags;
  std::shared_ptr<filt::SymbolTable> Symtab;
  std::shared_ptr<interp::IntStream> Contents;
  std::shared_ptr<interp::IntStream> IntOutput;
  bool ErrorsFound;
  void readInput();
  const decode::WriteCursor writeCodeOutput(
      std::shared_ptr<filt::SymbolTable> Symtab);
  void writeDataOutput(const decode::WriteCursor& StartPos,
                       std::shared_ptr<filt::SymbolTable> Symtab);
  bool compressUpToSize(size_t Size);
  void removeSmallUsageCounts();
  void assignInitialAbbreviations(CountNode::PtrVector& Assignments);
  bool generateIntOutput();
  std::shared_ptr<filt::SymbolTable>
  generateCode(CountNode::PtrVector& Assignments, bool ToRead, bool Trace);
  std::shared_ptr<filt::SymbolTable> generateCodeForReading(
      CountNode::PtrVector& Assignments) {
    return generateCode(Assignments, true,
                        MyFlags.TraceCodeGenerationForReading);
  }
  std::shared_ptr<filt::SymbolTable> generateCodeForWriting(
      CountNode::PtrVector& Assignments) {
    return generateCode(Assignments, false,
                        MyFlags.TraceCodeGenerationForWriting);
  }
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H
