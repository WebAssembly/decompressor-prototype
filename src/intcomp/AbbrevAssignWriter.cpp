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
    const AbbrevAssignFlags& MyFlags)
    : MyFlags(MyFlags),
      Root(Root),
      Writer(Output),
      BufSize(BufSize),
      Buffer(BufSize),
      AbbrevFormat(AbbrevFormat),
      AssumeByteAlignment(AssumeByteAlignment) {
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

CountNode::IntPtr AbbrevAssignWriter::extractMaxPattern(size_t StartIndex) {
  TRACE_METHOD("extractMaxPattern");
  CountNode::IntPtr Nd;
  CountNode::IntPtr Max;
  size_t EndIndex = std::min(StartIndex + BufSize, Buffer.size());
  constexpr bool AddIfNotFound = true;
  for (size_t i = StartIndex; i < EndIndex; ++i) {
    TRACE(size_t, "i", i);
    IntType Value = Buffer[i];
    TRACE(IntType, "Value", Value);
    Nd = Nd ? lookup(Nd, Value, !AddIfNotFound)
            : lookup(Root, Value, !AddIfNotFound);
    if (!Nd) {
      TRACE_MESSAGE("No more patterns found!");
      break;
    }
    if (!Nd->hasAbbrevIndex())
      continue;
#if 0
    // We should do this, but it makes things much worse, since we aren't
    // doing a good job at finding locality. For now, leave with bad code!
    if (!Max || (Max->getWeight() < Nd->getWeight()))
#else
    if (!Max || (Max->getWeight() > Nd->getWeight()))
#endif
    {
      Max = Nd;
      TRACE_BLOCK({
        FILE* Out = getTrace().getFile();
        fprintf(Out, "Max: ");
        Max->describe(Out);
      });
    }
  }
  return Max;
}

void AbbrevAssignWriter::writeFromBuffer() {
  TRACE_METHOD("writeFromBuffer");
  // TODO(karlschimpf): When writing values, dont' create abbreviation
  // if there are already default values, and adding as a default value
  // will use less space.
  if (Buffer.empty())
    return;
  TRACE_BLOCK({
    FILE* Out = getTrace().getFile();
    fprintf(Out, "** Buffer **\n");
    for (IntType Value : Buffer)
      fprintf(Out, "  %" PRIuMAX "\n", Value);
    fprintf(Out, "************\n");
  });
#if 1
  AbbrevSelector Selector(Buffer, Root, DefaultValues.size(),
                          DefaultFormat, MyFlags);
#if 1
  Selector.getTrace().setTraceProgress(getTrace().getTraceProgress());
#endif
  AbbrevSelection::Ptr Sel = Selector.select();
#endif
  // Collect abbreviations available for value sequences in buffer.
  CountNode::IntPtr Max = extractMaxPattern(0);
  // Before committing to Max, see if would be cheaper to just add
  // to default list.
  if (auto* MaxNd = dyn_cast<SingletonCountNode>(Max.get())) {
    // TODO(karlschimpf) Parameterize getValue cutoff as CL argument.
    constexpr IntType MaxLEBBytes = (IntType(1) << 3 * (CHAR_BIT - 1)) - 1;
    if (DefaultValues.size() >= 2 && MaxNd->getValue() <= MaxLEBBytes) {
      TRACE_MESSAGE("Ignore pattern, make other pattern!");
      Max = CountNode::IntPtr();
    }
  }
  if (MyFlags.CheckOverlapping && Max) {
    // See if overlappingn pattern is better choice.
    size_t EndIndex = Max->getPathLength();
    for (size_t i = 1; i < EndIndex; ++i) {
      if (i >= Buffer.size())
        break;
      CountNode::IntPtr CandMax = extractMaxPattern(i);
      // TODO(karlschimpf) Figure out heuristic to handle overlapping cost.
      if (CandMax && Max->getWeight() < CandMax->getWeight()) {
        Max = CountNode::IntPtr();
        break;
      }
    }
  }
  if (!Max) {
    // Default to writing at least one value.
    forwardOtherValue(Buffer[0]);
    popValuesFromBuffer(1);
    return;
  }
  forwardAbbrevValue(Max->getAbbrevIndex());
  popValuesFromBuffer(Max->getPathLength());
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
    FILE* Out = getTrace().getFile();
    fprintf(Out, "** Other values **\n");
    for (const IntType Value : DefaultValues)
      fprintf(Out, "  %" PRIuMAX "\n", Value);
    fprintf(Out, "******************\n");
  });

  if (DefaultValues.size() == 1) {
    IntType Abbrev = Root->getDefaultSingle()->getAbbrevIndex();
    TRACE(IntType, "Insert single abbrev", Abbrev);
    Writer.writeTypedValue(Abbrev, AbbrevFormat);
    TRACE(IntType, "Value", DefaultValues[0]);
    Writer.writeTypedValue(DefaultValues[0], DefaultFormat);
    DefaultValues.clear();
    return;
  }

  IntType Abbrev = Root->getDefaultMultiple()->getAbbrevIndex();
  TRACE(IntType, "Insert multiple abbrev", Abbrev);
  Writer.writeTypedValue(Abbrev, AbbrevFormat);
  TRACE(size_t, "Number values", DefaultValues.size());
  Writer.writeTypedValue(DefaultValues.size(), LoopSizeFormat);
  for (const IntType V : DefaultValues) {
    TRACE(IntType, "Value", V);
    Writer.writeTypedValue(V, DefaultFormat);
  }
  DefaultValues.clear();
}

}  // end of namespace intcomp

}  // end of namespace wasm
