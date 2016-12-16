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

// Defines a writer that injects abbreviations into the input stream.

#ifndef DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H
#define DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H

#include "intcomp/IntCountNode.h"
#include "interp/IntStream.h"
#include "interp/IntWriter.h"
#include "utils/circular-vector.h"

#include <vector>

namespace wasm {

namespace intcomp {

class AbbrevAssignWriter : public interp::Writer {
  AbbrevAssignWriter() = delete;
  AbbrevAssignWriter(const AbbrevAssignWriter&) = delete;
  AbbrevAssignWriter& operator=(const AbbrevAssignWriter&) = delete;

 public:
  AbbrevAssignWriter(CountNode::RootPtr Root,
                     std::shared_ptr<interp::IntStream> Output,
                     size_t BufSize,
                     interp::IntTypeFormat AbbrevFormat)
      : Root(Root),
        Writer(Output),
        Buffer(BufSize),
        AbbrevFormat(AbbrevFormat) {
    assert(Root->getDefaultSingle()->hasAbbrevIndex());
    assert(Root->getDefaultMultiple()->hasAbbrevIndex());
  }

  ~AbbrevAssignWriter() OVERRIDE {}

  decode::StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;
  void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace) OVERRIDE;

 private:
  CountNode::RootPtr Root;
  interp::IntWriter Writer;
  utils::circular_vector<decode::IntType> Buffer;
  interp::IntTypeFormat AbbrevFormat;
  std::vector<decode::IntType> DefaultValues;
  static constexpr interp::IntTypeFormat DefaultFormat =
      interp::IntTypeFormat::Varint64;
  static constexpr interp::IntTypeFormat LoopSizeFormat =
      interp::IntTypeFormat::Varuint64;

  void bufferValue(decode::IntType Value);
  void forwardAbbrevValue(decode::IntType Value);
  void forwardOtherValue(decode::IntType Value);
  void writeFromBuffer();
  void writeUntilBufferEmpty();
  void popValuesFromBuffer(size_t size);
  void flushDefaultValues();

  const char* getDefaultTraceName() const OVERRIDE;
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H
