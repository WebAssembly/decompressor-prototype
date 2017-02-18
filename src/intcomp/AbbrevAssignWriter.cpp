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

namespace {
enum class ValueType { Abbreviation, Default, Loop };
}  // end of anonymous namespace

// Base class for assignment values to be written.
class AbbrevAssignValue {
  AbbrevAssignValue() = delete;
  AbbrevAssignValue(const AbbrevAssignValue&) = delete;
  AbbrevAssignValue& operator=(const AbbrevAssignValue&) = delete;

 public:
  virtual ~AbbrevAssignValue();
  virtual void describe(FILE* Out) = 0;
  ValueType getKind() const { return Kind; }

  // The following define casting oeprations isa<>, dyn_cast<>, and cast<>
  ValueType getRtClassId() const { return Kind; }
  static bool implementsClass(ValueType Kind) { return true; }

 protected:
  ValueType Kind;
  AbbrevAssignValue(ValueType Kind) : Kind(Kind) {}
};

AbbrevAssignValue::~AbbrevAssignValue() {
}

namespace {

class AbbrevValue : public AbbrevAssignValue {
  AbbrevValue() = delete;
  AbbrevValue(const AbbrevValue&) = delete;
  AbbrevValue& operator=(const AbbrevValue&);

 public:
  ~AbbrevValue() OVERRIDE;
  void describe(FILE* Out) OVERRIDE;
  static AbbrevValue* create(CountNode::Ptr Abbreviation);
  CountNode::Ptr getAbbreviation() const { return Abbreviation; }
  static bool implementsClass(ValueType Kind) {
    return Kind == ValueType::Abbreviation;
  }

 private:
  CountNode::Ptr Abbreviation;
  explicit AbbrevValue(CountNode::Ptr Abbreviation)
      : AbbrevAssignValue(ValueType::Abbreviation),
        Abbreviation(Abbreviation) {}
};

AbbrevValue::~AbbrevValue() {
}

AbbrevValue* AbbrevValue::create(CountNode::Ptr Abbreviation) {
  return new AbbrevValue(Abbreviation);
}

void AbbrevValue::describe(FILE* Out) {
  fprintf(Out, "Abbrev: ");
  Abbreviation->describe(Out);
}

class IntValue : public AbbrevAssignValue {
  IntValue() = delete;
  IntValue(const IntValue&) = delete;
  IntValue& operator=(const IntValue&) = delete;

 public:
  ~IntValue() OVERRIDE;
  IntType getValue() const { return Value; }
  static bool implementsClass(ValueType Kind) {
    return Kind != ValueType::Abbreviation;
  }

 protected:
  IntValue(ValueType Kind, IntType Value)
      : AbbrevAssignValue(Kind), Value(Value) {}

 private:
  const IntType Value;
};

IntValue::~IntValue() {
}

class DefaultValue : public IntValue {
  DefaultValue() = delete;
  DefaultValue(const DefaultValue&) = delete;
  DefaultValue& operator=(const DefaultValue&) = delete;

 public:
  ~DefaultValue() OVERRIDE;
  void describe(FILE* OUt) OVERRIDE;
  static DefaultValue* create(IntType Value);
  static bool implementsClass(ValueType Kind) {
    return Kind == ValueType::Default;
  }

 private:
  DefaultValue(IntType Value) : IntValue(ValueType::Default, Value) {}
};

DefaultValue::~DefaultValue() {
}

DefaultValue* DefaultValue::create(IntType Value) {
  return new DefaultValue(Value);
}

void DefaultValue::describe(FILE* Out) {
  fprintf(Out, "Default: %" PRIuMAX "\n", getValue());
}

class LoopValue : public IntValue {
  LoopValue() = delete;
  LoopValue(const LoopValue&) = delete;
  LoopValue& operator=(const LoopValue&) = delete;

 public:
  ~LoopValue() OVERRIDE;
  void describe(FILE* Out) OVERRIDE;
  static LoopValue* create(size_t Value);
  static bool implementsClass(ValueType Kind) {
    return Kind == ValueType::Loop;
  }

 private:
  LoopValue(decode::IntType Value) : IntValue(ValueType::Loop, Value) {}
};

LoopValue::~LoopValue() {
}

LoopValue* LoopValue::create(IntType Value) {
  return new LoopValue(Value);
}

void LoopValue::describe(FILE* Out) {
  fprintf(Out, "Size: %" PRIuMAX "\n", getValue());
}

}  // end of anonymous namespace

AbbrevAssignWriter::AbbrevAssignWriter(
    CountNode::RootPtr Root,
    CountNode::PtrSet& Assignments,
    std::shared_ptr<interp::IntStream> Output,
    size_t BufSize,
    bool AssumeByteAlignment,
    const CompressionFlags& MyFlags)
    : MyFlags(MyFlags),
      Root(Root),
#if 0
      Assignments(Assignments),
#endif
      Writer(Output),
      Buffer(BufSize),
      AssumeByteAlignment(AssumeByteAlignment),
      ProgressCount(0) {
#if 1
  (void)Assignments;
#endif
  assert(Root->getDefaultSingle()->hasAbbrevIndex());
  assert(Root->getDefaultMultiple()->hasAbbrevIndex());
}

AbbrevAssignWriter::~AbbrevAssignWriter() {
  clearValues();
}

void AbbrevAssignWriter::clearValues() {
  for (AbbrevAssignValue* Value : Values)
    delete Value;
  Values.clear();
}

const char* AbbrevAssignWriter::getDefaultTraceName() const {
  return "AbbrevAssignWriter";
}

void AbbrevAssignWriter::forwardAbbrev(CountNode::Ptr Abbrev) {
  flushDefaultValues();
  forwardAbbrevAfterFlush(Abbrev);
}

void AbbrevAssignWriter::forwardAbbrevAfterFlush(CountNode::Ptr Abbrev) {
  assert(Abbrev->hasAbbrevIndex());
  TRACE_BLOCK({
    TRACE_PREFIX("Insert ");
    Abbrev->describe(getTrace().getFile());
  });
  Values.push_back(AbbrevValue::create(Abbrev));
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

bool AbbrevAssignWriter::writeVaruint64(uint64_t Value) {
  bufferValue(Value);
  return true;
}

void AbbrevAssignWriter::alignIfNecessary() {
  if (AssumeByteAlignment)
    return;
  forwardAbbrev(Root->getAlign());
}

bool AbbrevAssignWriter::writeFreezeEof() {
  writeUntilBufferEmpty();
  flushDefaultValues();
  alignIfNecessary();
  return flushValues();
}

bool AbbrevAssignWriter::flushValues() {
  TRACE_MESSAGE("Flushing collected abbreviations");
  for (AbbrevAssignValue* Value : Values) {
    TRACE_BLOCK({
      TRACE_PREFIX("Write ");
      Value->describe(getTrace().getFile());
    });
    switch (Value->getKind()) {
      default:
        fprintf(
            stderr,
            "Internal error found while flushing Abbreviation assignments\n");
        return false;
      case ValueType::Abbreviation: {
        AbbrevValue* Abbrev = cast<AbbrevValue>(Value);
        Writer.write(Abbrev->getAbbreviation()->getAbbrevIndex());
        break;
      }
      case ValueType::Default: {
        DefaultValue* Default = cast<DefaultValue>(Value);
        Writer.write(Default->getValue());
        break;
      }
      case ValueType::Loop: {
        LoopValue* Loop = cast<LoopValue>(Value);
        Writer.write(Loop->getValue());
      }
    }
  }
  return Writer.writeFreezeEof();
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
      forwardAbbrev(Root->getBlockEnter());
      return true;
    case PredefinedSymbol::Block_exit:
      writeUntilBufferEmpty();
      flushDefaultValues();
      forwardAbbrev(Root->getBlockExit());
      return true;
    default:
      // There should not be any other actions!!
      return false;
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
        forwardAbbrev(Sel->getAbbreviation());
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
    forwardAbbrevAfterFlush(Root->getDefaultSingle());
    IntType Value = DefaultValues[0];
    TRACE(IntType, "Value", Value);
    Values.push_back(DefaultValue::create(Value));
    DefaultValues.clear();
    return;
  }

  forwardAbbrevAfterFlush(Root->getDefaultMultiple());
  Values.push_back(LoopValue::create(DefaultValues.size()));
  for (const IntType Value : DefaultValues) {
    TRACE(IntType, "Value", Value);
    Values.push_back(DefaultValue::create(Value));
  }
  DefaultValues.clear();
}

}  // end of namespace intcomp

}  // end of namespace wasm
