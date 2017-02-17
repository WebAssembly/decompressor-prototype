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

// Implements a writer that injects abbreviations into the input stream.

#include "intcomp/AbbrevAssignWriter.h"
#include "intcomp/AbbrevSelector.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace intcomp {

AbbrevAssignWriter::AbbrevAssignWriter(
    CountNode::RootPtr Root,
    std::shared_ptr<interp::IntStream> Output,
    size_t BufSize,
    interp::IntTypeFormat AbbrevFormat,
    bool AssumeByteAlignment,
    const CompressionFlags& MyFlags)
    : MyFlags(MyFlags),
      Root(Root),
      Writer(Output),
      Buffer(BufSize),
      AbbrevFormat(AbbrevFormat),
      AssumeByteAlignment(AssumeByteAlignment),
      ProgressCount(0) {
  assert(Root->getDefaultSingle()->hasAbbrevIndex());
  assert(Root->getDefaultMultiple()->hasAbbrevIndex());
}

AbbrevAssignWriter::~AbbrevAssignWriter() {
}

const char* AbbrevAssignWriter::getDefaultTraceName() const {
  return "AbbrevAssignWriter";
}

void AbbrevAssignWriter::forwardAbbrevValue(IntType Value) {
  flushDefaultValues();
  TRACE(IntType, "Insert abbreviation", Value);
  Writer.writeTypedValue(Value, AbbrevFormat);
}

void AbbrevAssignWriter::forwardOtherValue(IntType Value) {
  TRACE(IntType, "Forward other", Value);
  DefaultValues.push_back(Value);
}

void AbbrevAssignWriter::setTrace(std::shared_ptr<TraceClass> Trace) {
  Writer::setTrace(Trace);
  Writer.setTrace(Trace);
}

StreamType AbbrevAssignWriter::getStreamType() const {
  return StreamType::Int;
}

bool AbbrevAssignWriter::writeUint8(uint8_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeUint32(uint32_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeUint64(uint64_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeVarint32(int32_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeVarint64(int64_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeVaruint32(uint32_t Value) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeVaruint64(uint64_t Value) {
  bufferValue(Value);
  return true;
}

void AbbrevAssignWriter::alignIfNecessary() {
  if (AssumeByteAlignment)
    return;
  assert(Root->getAlign()->hasAbbrevIndex());
  forwardAbbrevValue(Root->getAlign()->getAbbrevIndex());
}

bool AbbrevAssignWriter::writeFreezeEof() {
  writeUntilBufferEmpty();
  flushDefaultValues();
  alignIfNecessary();
  return Writer.writeFreezeEof();
}

bool AbbrevAssignWriter::writeValue(decode::IntType Value, const filt::Node*) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeHeaderValue(decode::IntType Value,
                                          interp::IntTypeFormat Format) {
  return Writer.writeHeaderValue(Value, Format);
}

bool AbbrevAssignWriter::writeAction(const filt::SymbolNode* Action) {
  switch (Action->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
      writeUntilBufferEmpty();
      flushDefaultValues();
      assert(Root->getBlockEnter()->hasAbbrevIndex());
      forwardAbbrevValue(Root->getBlockEnter()->getAbbrevIndex());
      return true;
    case PredefinedSymbol::Block_exit:
      writeUntilBufferEmpty();
      flushDefaultValues();
      assert(Root->getBlockExit()->hasAbbrevIndex());
      forwardAbbrevValue(Root->getBlockExit()->getAbbrevIndex());
      return true;
    default:
      return Writer.writeAction(Action);
  }
}

void AbbrevAssignWriter::bufferValue(IntType Value) {
  TRACE(IntType, "Buffer.enqueue", Value);
  assert(!Buffer.full());
  Buffer.push_back(Value);
  if (!Buffer.full())
    return;
  writeFromBuffer();
}

void AbbrevAssignWriter::writeFromBuffer() {
  TRACE_METHOD("writeFromBuffer");
  // TODO(karlschimpf): When writing values, dont' create abbreviation
  // if there are already default values, and adding as a default value
  // will use less space.
  if (Buffer.empty())
    return;
  TRACE_BLOCK({
    if (MyFlags.TraceAbbrevSelectionDetail) {
      FILE* Out = getTrace().getFile();
      fprintf(Out, "** Buffer **\n");
      for (IntType Value : Buffer)
        fprintf(Out, "  %" PRIuMAX "\n", Value);
      fprintf(Out, "************\n");
    }
  });
  AbbrevSelector Selector(Buffer, Root, DefaultValues.size(), MyFlags);
  Selector.setTrace(getTracePtr());
  AbbrevSelection::Ptr Sel = Selector.select();
  // Report progress...
  // TODO(karlschimp): Figure out why TRACE macro can't be used!
  if (MyFlags.TraceAbbrevSelectionProgress != 0) {
    size_t Gap = MyFlags.TraceAbbrevSelectionProgress;
    size_t Count = Writer.getIndex();
    while (Count >= ProgressCount + Gap) {
      ProgressCount += Gap;
      fprintf(stderr, "Progress: %" PRIuMAX "\n", uintmax_t(ProgressCount));
    }
  }
  // Flatten selected sequence (in order) so that we can apply leading selected
  // patterns. Note: Do not want to choose all in that the whole point of
  // selecting on a window is to see effects of nearby integers in the integer
  // seelection. On the other hand, finding selections is expensive, so we want
  // to consume as much of the buffer as possible.  This code mitigates this by
  // flushing up to the pattern length limit.
  std::vector<AbbrevSelection::Ptr> Selections;
  while (Sel) {
    Selections.push_back(Sel);
    Sel = Sel->getPrevious();
  }
  size_t Index = 0;
  while (!Selections.empty() && Index < MyFlags.PatternLengthLimit) {
    AbbrevSelection::Ptr Sel = Selections.back();
    Selections.pop_back();
    CountNode* Abbrev = Sel->getAbbreviation().get();
    switch (Abbrev->getKind()) {
      case CountNode::Kind::Default:
        forwardOtherValue(Buffer[0]);
        popValuesFromBuffer(1);
        ++Index;
        break;
      case CountNode::Kind::Singleton:
      case CountNode::Kind::IntSequence: {
        IntCountNode* IntAbbrev = cast<IntCountNode>(Abbrev);
        size_t Length = IntAbbrev->getPathLength();
        if (Index > 0 && (Index + Length) > MyFlags.PatternLengthLimit) {
          Selections.clear();
          break;
        }
        forwardAbbrevValue(IntAbbrev->getAbbrevIndex());
        popValuesFromBuffer(Length);
        Index += Length;
        break;
      }
      default:
        assert(false);
        break;
    }
  }
}

void AbbrevAssignWriter::writeUntilBufferEmpty() {
  while (!Buffer.empty())
    writeFromBuffer();
}

void AbbrevAssignWriter::popValuesFromBuffer(size_t Size) {
  for (size_t i = 0; i < Size; ++i) {
    if (Buffer.empty())
      return;
    TRACE(IntType, "Buffer.deque", Buffer.front());
    Buffer.pop_front();
  }
}

void AbbrevAssignWriter::flushDefaultValues() {
  if (DefaultValues.empty())
    return;
  TRACE_METHOD("flushDefaultValues");
  TRACE_BLOCK({
    if (MyFlags.TraceAbbrevSelectionDetail) {
      FILE* Out = getTrace().getFile();
      fprintf(Out, "** Other values **\n");
      for (const IntType Value : DefaultValues)
        fprintf(Out, "  %" PRIuMAX "\n", Value);
      fprintf(Out, "******************\n");
    }
  });

  if (DefaultValues.size() == 1) {
    IntType Abbrev = Root->getDefaultSingle()->getAbbrevIndex();
    TRACE(IntType, "Insert single abbrev", Abbrev);
    Writer.writeTypedValue(Abbrev, AbbrevFormat);
    TRACE(IntType, "Value", DefaultValues[0]);
    Writer.writeTypedValue(DefaultValues[0], MyFlags.DefaultFormat);
    DefaultValues.clear();
    return;
  }

  IntType Abbrev = Root->getDefaultMultiple()->getAbbrevIndex();
  TRACE(IntType, "Insert multiple abbrev", Abbrev);
  Writer.writeTypedValue(Abbrev, AbbrevFormat);
  TRACE(size_t, "Number values", DefaultValues.size());
  Writer.writeTypedValue(DefaultValues.size(), MyFlags.LoopSizeFormat);
  for (const IntType V : DefaultValues) {
    TRACE(IntType, "Value", V);
    Writer.writeTypedValue(V, MyFlags.DefaultFormat);
  }
  DefaultValues.clear();
}

}  // end of namespace intcomp

}  // end of namespace wasm
