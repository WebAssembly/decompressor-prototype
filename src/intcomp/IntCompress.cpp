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

// Implements the compressor of WASM fiels base on integer usage.

#include "intcomp/IntCompress.h"
#include "interp/IntReader.h"
#include "interp/IntWriter.h"
#include "interp/StreamReader.h"
#include "utils/heap.h"
#include "utils/circular-vector.h"

#include <algorithm>
#include <vector>
#include <set>

#define DESCRIBE_INPUT 0

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;

namespace intcomp {

// Warning: This code assumes that multiple passes are used by the writer, where
// each pass calls setUpToSize(N) with a value N. Further, the first pass always
// uses N=1.  Failure to perform an initial write with setUpToSize(1) will
// result in very bad behaviour.
class IntCounterWriter : public Writer {
  IntCounterWriter() = delete;
  IntCounterWriter(const IntCounterWriter&) = delete;
  IntCounterWriter& operator=(const IntCounterWriter&) = delete;

 public:
  typedef std::vector<CountNode::IntPtr> IntFrontier;
  typedef std::set<CountNode::IntPtr> CountNodeIntSet;
  IntCounterWriter(CountNode::RootPtr Root)
      : Root(Root), CountCutoff(1), UpToSize(0) {}

  ~IntCounterWriter() OVERRIDE;

  void setCountCutoff(uint64_t NewValue) { CountCutoff = NewValue; }
  void setUpToSize(size_t NewSize) {
    assert(NewSize >= 1);
    UpToSize = NewSize;
  }
  void resetUpToSize() { UpToSize = 0; }
  size_t getUpToSize() const { return UpToSize; }

  void addToUsageMap(IntType Value);

  StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

 private:
  CountNode::RootPtr Root;
  IntFrontier Frontier;
  uint64_t CountCutoff;
  size_t UpToSize;
};

IntCounterWriter::~IntCounterWriter() {
}

StreamType IntCounterWriter::getStreamType() const {
  return StreamType::Int;
}

void IntCounterWriter::addToUsageMap(IntType Value) {
  CountNode::IntPtr TopNd = lookup(Root, Value);
  if (UpToSize == 1) {
    TopNd->increment();
    return;
  }
  IntFrontier NextFrontier;
  while (!Frontier.empty()) {
    CountNode::IntPtr Nd = Frontier.back();
    Frontier.pop_back();
    if (Nd->getPathLength() >= UpToSize || TopNd->getWeight() < CountCutoff)
      continue;
    Nd = lookup(Nd, Value);
    Nd->increment();
    NextFrontier.push_back(Nd);
  }
  Frontier.swap(NextFrontier);
  if (TopNd->getWeight() >= CountCutoff)
    Frontier.push_back(TopNd);
}

bool IntCounterWriter::writeUint8(uint8_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeUint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeUint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVarint32(int32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVarint64(int64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVaruint32(uint32_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeVaruint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeValue(decode::IntType Value, const filt::Node*) {
  addToUsageMap(Value);
  return true;
}

bool IntCounterWriter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
      Frontier.clear();
      Root->getBlockEnter()->increment();
      return true;
    case PredefinedSymbol::Block_exit:
      Frontier.clear();
      Root->getBlockExit()->increment();
      return true;
    default:
      return true;
  }
}

class AbbrevAssignWriter : public Writer {
  AbbrevAssignWriter() = delete;
  AbbrevAssignWriter(const AbbrevAssignWriter&) = delete;
  AbbrevAssignWriter& operator=(const AbbrevAssignWriter&) = delete;

 public:
  typedef std::pair<IntType, IntType> ValuePair;
  AbbrevAssignWriter(CountNode::RootPtr Root,
                     std::shared_ptr<IntStream> Output,
                     size_t BufSize,
                     IntTypeFormat AbbrevFormat)
      : Root(Root),
        Writer(Output),
        Buffer(BufSize),
        AbbrevFormat(AbbrevFormat) {
    assert(Root->getDefaultSingle()->hasAbbrevIndex());
    assert(Root->getDefaultMultiple()->hasAbbrevIndex());
  }

  ~AbbrevAssignWriter() OVERRIDE {}

  StreamType getStreamType() const OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

 private:
  CountNode::RootPtr Root;
  IntWriter Writer;
  circular_vector<ValuePair> Buffer;
  IntTypeFormat AbbrevFormat;
  std::vector<IntType> DefaultValues;
  static constexpr IntTypeFormat DefaultFormat = IntTypeFormat::Varint64;
  static constexpr IntTypeFormat LoopSizeFormat = IntTypeFormat::Varuint64;

  void bufferValue(IntType Value);
  void forwardAbbrevValue(IntType Value) {
    flushDefaultValues();
    writeValue(Value, AbbrevFormat);
  }
  void forwardOtherValue(IntType Value) { DefaultValues.push_back(Value); }
  void forwardValue(ValuePair Pair) {
    if (CountNode::isAbbrevDefined(Pair.second))
      forwardAbbrevValue(Pair.first);
    else
      forwardOtherValue(Pair.first);
  }
  void writeFromBuffer();
  void writeUntilBufferEmpty();
  void popValuesFromBuffer(size_t size);
  void flushDefaultValues();
  void writeValue(IntType Value, IntTypeFormat Format);
};

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

bool AbbrevAssignWriter::writeValue(decode::IntType Value, const filt::Node*) {
  bufferValue(Value);
  return true;
}

bool AbbrevAssignWriter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_exit:
      writeUntilBufferEmpty();
    // Intentionally drop to default case.
    default:
      Writer.writeAction(Action);
      return true;
  }
}

void AbbrevAssignWriter::bufferValue(IntType Value) {
  assert(!Buffer.full());
  Buffer.push_back(std::make_pair(Value, CountNode::BAD_ABBREV_INDEX));
  if (!Buffer.full())
    return;
  writeFromBuffer();
}

void AbbrevAssignWriter::writeFromBuffer() {
  // TODO(karlschimpf): When writing values, dont' create abbreviation
  // if there are already default values, and adding as a default value
  // will use less space.
  if (Buffer.empty())
    return;
  // Collect abbreviations available for value sequences in buffer.
  CountNode::IntPtr Nd;
  size_t MaxIndex = 0;
  for (size_t e = Buffer.size(); MaxIndex < e; ++MaxIndex) {
    ValuePair& Pair = Buffer[MaxIndex];
    Nd = MaxIndex ? lookup(Nd, Pair.first) : lookup(Root, Pair.first);
    Pair.second = Nd->getAbbrevIndex();
  }
  // Choose maximal abbreviation (if possible).
  do {
    size_t Index = MaxIndex--;
    if (CountNode::isAbbrevDefined(Buffer[Index].second))
      continue;
    forwardValue(Buffer[Index]);
    popValuesFromBuffer(Index + 1);
    return;
  } while (MaxIndex > 0);
  // Default to writing at least one value.
  forwardValue(Buffer[0]);
  popValuesFromBuffer(1);
}

void AbbrevAssignWriter::writeUntilBufferEmpty() {
  while (!Buffer.empty())
    writeFromBuffer();
}

void AbbrevAssignWriter::popValuesFromBuffer(size_t Size) {
  for (size_t i = 0; i < Size; ++i) {
    if (Buffer.empty())
      return;
    Buffer.pop_front();
  }
}

void AbbrevAssignWriter::writeValue(IntType Value, IntTypeFormat Format) {
  switch (Format) {
    case IntTypeFormat::Uint8:
      Writer.writeUint8(Value);
      break;
    case IntTypeFormat::Uint32:
      Writer.writeUint32(Value);
      break;
    case IntTypeFormat::Uint64:
      Writer.writeUint64(Value);
      break;
    case IntTypeFormat::Varint32:
      Writer.writeVarint32(Value);
      break;
    case IntTypeFormat::Varint64:
      Writer.writeVarint64(Value);
      break;
    case IntTypeFormat::Varuint32:
      Writer.writeVaruint32(Value);
      break;
    case IntTypeFormat::Varuint64:
      Writer.writeVaruint64(Value);
  }
}

void AbbrevAssignWriter::flushDefaultValues() {
  if (DefaultValues.empty())
    return;

  if (DefaultValues.size() == 1) {
    writeValue(Root->getDefaultSingle()->getAbbrevIndex(), AbbrevFormat);
    writeValue(DefaultValues[0], DefaultFormat);
    DefaultValues.clear();
    return;
  }

  writeValue(Root->getDefaultMultiple()->getAbbrevIndex(), AbbrevFormat);
  writeValue(DefaultValues.size(), LoopSizeFormat);
  for (const auto V : DefaultValues)
    writeValue(V, DefaultFormat);
  DefaultValues.clear();
}

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Input(Input),
      Output(Output),
      Symtab(Symtab),
      CountCutoff(0),
      WeightCutoff(0),
      LengthLimit(1),
      AbbrevFormat(IntTypeFormat::Varuint64),
      ErrorsFound(false) {
}

void IntCompressor::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClassSexp> IntCompressor::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("IntCompress"));
  return Trace;
}

CountNode::RootPtr IntCompressor::getRoot() {
  if (!Root)
    Root = std::make_shared<RootCountNode>();
  return Root;
}

void IntCompressor::readInput(std::shared_ptr<Queue> InputStream,
                              std::shared_ptr<SymbolTable> Symtab,
                              bool TraceParsing) {
  Contents = std::make_shared<IntStream>();
  IntWriter MyWriter(Contents);
  StreamReader MyReader(InputStream, MyWriter, Symtab);
  if (TraceParsing) {
    TraceClassSexp& Trace = MyReader.getTrace();
    Trace.addContext(MyReader.getTraceContext());
    Trace.addContext(MyWriter.getTraceContext());
    Trace.setTraceProgress(true);
  }
  MyReader.fastStart();
  MyReader.fastReadBackFilled();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  Input.reset();
  return;
}

IntCompressor::~IntCompressor() {
}

bool IntCompressor::compressUpToSize(size_t Size, bool TraceParsing) {
  TRACE_BLOCK({
    if (Size == 1)
      TRACE_MESSAGE("Collecting integer sequences of length: 1");
    else
      TRACE_MESSAGE("Collecting integer sequences of (up to) length: " +
                    std::to_string(Size));
  });
  IntCounterWriter Writer(getRoot());
  Writer.setCountCutoff(CountCutoff);
  Writer.setUpToSize(Size);
  IntReader Reader(Contents, Writer, Symtab);
  if (TraceParsing)
    Reader.getTrace().setTraceProgress(true);
  Reader.fastStart();
  Reader.fastReadBackFilled();
  return !Reader.errorsFound();
}

namespace {

class RemoveNodesVisitor : public CountNodeVisitor {
  RemoveNodesVisitor() = delete;
  RemoveNodesVisitor(const RemoveNodesVisitor&) = delete;
  RemoveNodesVisitor& operator=(const RemoveNodesVisitor&) = delete;

 public:
  class Frame : public CountNodeVisitor::Frame {
    Frame() = delete;
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    friend class RemoveNodesVisitor;

   public:
    Frame(CountNodeVisitor& Visitor, size_t FirstKid, size_t LastKid)
        : CountNodeVisitor::Frame(Visitor, FirstKid, LastKid) {}

    Frame(CountNodeVisitor& Visitor,
          CountNode::IntPtr Nd,
          size_t FirstKid,
          size_t LastKid)
        : CountNodeVisitor::Frame(Visitor, Nd, FirstKid, LastKid) {
      assert(Nd);
    }

    ~Frame() OVERRIDE {}

    void describeSuffix(FILE* Out) const OVERRIDE;

   private:
    std::vector<CountNode::IntPtr> RemoveSet;
  };
  explicit RemoveNodesVisitor(CountNode::RootPtr Root,
                              size_t CountCutoff,
                              size_t PatternCutoff = 2)
      : CountNodeVisitor(Root),
        CountCutoff(CountCutoff),
        PatternCutoff(PatternCutoff) {}
  ~RemoveNodesVisitor() {}

 protected:
  size_t CountCutoff;
  size_t PatternCutoff;
  FramePtr getFrame(size_t FirstKid, size_t LastKid) OVERRIDE;
  FramePtr getFrame(CountNode::IntPtr Nd,
                    size_t FirstKid,
                    size_t LastKid) OVERRIDE;

  void visit(FramePtr Frame) OVERRIDE;
  void visitReturn(FramePtr Frame) OVERRIDE;
};

CountNodeVisitor::FramePtr RemoveNodesVisitor::getFrame(size_t FirstKid,
                                                        size_t LastKid) {
  return std::make_shared<RemoveNodesVisitor::Frame>(*this, FirstKid, LastKid);
}

CountNodeVisitor::FramePtr RemoveNodesVisitor::getFrame(CountNode::IntPtr Nd,
                                                        size_t FirstKid,
                                                        size_t LastKid) {
  return std::make_shared<RemoveNodesVisitor::Frame>(*this, Nd, FirstKid,
                                                     LastKid);
}

void RemoveNodesVisitor::visit(FramePtr Frame) {
  auto* F = reinterpret_cast<RemoveNodesVisitor::Frame*>(Frame.get());
  while (!F->RemoveSet.empty()) {
    CountNode::IntPtr Nd = F->RemoveSet.back();
    F->RemoveSet.pop_back();
    if (isa<SingletonCountNode>(*Nd))
      getRoot()->getDefaultSingle()->increment(Nd->getCount());
    F->getNode()->eraseSucc(Nd->getValue());
  }
}

void RemoveNodesVisitor::visitReturn(FramePtr Frame) {
  auto* F = reinterpret_cast<RemoveNodesVisitor::Frame*>(Frame.get());
  if (!F->isIntNodeFrame())
    return;
  CountNode::IntPtr Nd = F->getIntNode();
  if (!(Nd->hasSuccessors() ||
        (Nd->getWeight() >= CountCutoff && Nd->getCount() >= PatternCutoff))) {
    reinterpret_cast<RemoveNodesVisitor::Frame*>(Stack.back().get())
        ->RemoveSet.push_back(Nd);
  }
}

void RemoveNodesVisitor::Frame::describeSuffix(FILE* Out) const {
  if (!RemoveSet.empty()) {
    fputc('\n', Out);
    for (auto Nd : RemoveSet) {
      fputs("  ", Out);
      Nd->describe(Out);
    }
  }
  CountNodeVisitor::Frame::describeSuffix(Out);
}

}  // end of anonymous namespace

void IntCompressor::removeSmallUsageCounts() {
  // NOTE: The main purpose of this method is to shrink the size of
  // the trie to (a) recover memory and (b) make remaining analysis
  // faster.  It does this by removing int count nodes that are not
  // not useful (See case RemoveFrame::State::Exit for details).
  RemoveNodesVisitor Visitor(Root, CountCutoff);
  Visitor.walk();
}

void IntCompressor::compress(DetailLevel Level,
                             bool TraceParsing,
                             bool TraceFirstPassOnly) {
  TRACE_METHOD("compress");
  readInput(Input, Symtab, TraceParsing);
  if (errorsFound()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  TRACE(size_t, "Number of integers in input", Contents->getNumIntegers());
  if (Level == DetailLevel::AllDetail)
    Contents->describe(stderr, "Input int stream");
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // trie.
  if (!compressUpToSize(1, TraceParsing && !TraceFirstPassOnly))
    return;
  if (Level >= DetailLevel::MoreDetail)
    describe(stderr, makeFlags(CollectionFlag::TopLevel));
  removeSmallUsageCounts();
  if (LengthLimit > 1) {
    if (!compressUpToSize(LengthLimit, TraceParsing && !TraceFirstPassOnly))
      return;
    removeSmallUsageCounts();
  }
  if (Level >= DetailLevel::MoreDetail)
    describe(stderr, makeFlags(CollectionFlag::IntPaths));
  assignInitialAbbreviations();
  if (Level > DetailLevel::NoDetail)
    describe(stderr, makeFlags(CollectionFlag::All));
}

namespace {

class CountNodeCollector {
 public:
  CountNode::RootPtr Root;
  std::vector<CountNode::HeapValueType> Values;
  std::shared_ptr<CountNode::HeapType> ValuesHeap;
  uint64_t WeightTotal;
  uint64_t CountTotal;
  uint64_t WeightReported;
  uint64_t CountReported;
  uint64_t NumNodesReported;
  uint64_t CountCutoff;
  uint64_t WeightCutoff;
  std::shared_ptr<filt::TraceClassSexp> Trace;

  explicit CountNodeCollector(CountNode::RootPtr Root)
      : Root(Root),
        ValuesHeap(std::make_shared<CountNode::HeapType>()),
        WeightTotal(0),
        CountTotal(0),
        WeightReported(0),
        CountReported(0),
        NumNodesReported(0),
        CountCutoff(1),
        WeightCutoff(1) {}
  ~CountNodeCollector() { clear(); }

  void collect(CollectionFlags Flags = makeFlags(CollectionFlag::All));
  void buildHeap();
  CountNode::HeapValueType popHeap() {
    assert(ValuesHeap);
    CountNode::HeapEntryType Entry = ValuesHeap->top();
    ValuesHeap->pop();
    return Entry->getValue();
  }
  void clearHeap();
  void describeHeap(FILE* Out) {
    ValuesHeap->describe(Out, [](FILE* Out, CountNode::HeapValueType Value) {
      Value->describe(Out);
    });
  }
  void clear();
  void collectNode(CountNode::Ptr Nd, CollectionFlags Flags);

  void setTrace(std::shared_ptr<filt::TraceClassSexp> Trace);
  filt::TraceClassSexp& getTrace();
  bool hasTrace() { return bool(Trace); }
  void describe(FILE* Out);
};

void CountNodeCollector::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
}

filt::TraceClassSexp& CountNodeCollector::getTrace() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("IntCompress"));
  return *Trace;
}

void CountNodeCollector::clearHeap() {
  for (auto& Value : Values)
    Value->disassociateFromHeap();
}

void CountNodeCollector::clear() {
  clearHeap();
  Values.clear();
  ValuesHeap.reset();
}

void CountNodeCollector::buildHeap() {
  for (auto& Value : Values)
    Value->associateWithHeap(ValuesHeap->push(Value));
}

void CountNodeCollector::collect(CollectionFlags Flags) {
  if (hasFlag(CollectionFlag::TopLevel, Flags)) {
    collectNode(Root->getBlockEnter(), Flags);
    collectNode(Root->getBlockExit(), Flags);
    collectNode(Root->getDefaultSingle(), Flags);
    collectNode(Root->getDefaultMultiple(), Flags);
  }
  for (CountNode::SuccMapIterator Iter = Root->getSuccBegin(),
                                  End = Root->getSuccEnd();
       Iter != End; ++Iter)
    collectNode(Iter->second, Flags);
}

void CountNodeCollector::collectNode(CountNode::Ptr Nd, CollectionFlags Flags) {
  bool IsIntNode = isa<IntCountNode>(*Nd);
  std::vector<CountNode::Ptr> ToAdd;
  ToAdd.push_back(Nd);
  while (!ToAdd.empty()) {
    Nd = ToAdd.back();
    ToAdd.pop_back();
    if (!Nd)  // This shouldn't happen, but be safe.
      continue;
    if (auto* IntNd = dyn_cast<IntCountNode>(*Nd))
      for (CountNode::SuccMapIterator Iter = IntNd->getSuccBegin(),
                                      End = IntNd->getSuccEnd();
           Iter != End; ++Iter)
        ToAdd.push_back(Iter->second);
    uint64_t Weight = Nd->getWeight();
    size_t Count = Nd->getCount();
    bool IsSingleton = !IsIntNode || isa<SingletonCountNode>(*Nd);
    auto* IntNd = dyn_cast<IntCountNode>(Nd.get());
    if (hasFlag(CollectionFlag::TopLevel, Flags)) {
      CountTotal += Count;
      WeightTotal += Weight;
      if (IsIntNode) {
        if (Count < CountCutoff)
          continue;
        if (Weight < WeightCutoff)
          continue;
      }
      if (IntNd == nullptr || IsSingleton) {
        CountReported += Count;
        WeightReported += Weight;
        ++NumNodesReported;
        Values.push_back(Nd);
      }
    }
    if (IntNd == nullptr || !hasFlag(CollectionFlag::IntPaths, Flags))
      continue;
    if (!IsSingleton) {
      CountTotal += Count;
      WeightTotal += Weight;
    }
    if (IsIntNode) {
      if (Count < CountCutoff)
        continue;
      if (Weight < WeightCutoff)
        continue;
    }
    if (!IsSingleton) {
      Values.push_back(CountNode::Ptr(Nd));
      CountReported += Count;
      WeightReported += Weight;
      ++NumNodesReported;
    }
  }
}

void CountNodeCollector::describe(FILE* Out) {
  assert(ValuesHeap->empty());
  buildHeap();
  fprintf(Out, "Number nodes reported: %" PRIuMAX
               "\n"
               "Total weight: %" PRIuMAX " Reported Weight %" PRIuMAX
               "\n"
               "Total count: %" PRIuMAX " Reported count %" PRIuMAX "\n",
          uintmax_t(NumNodesReported), uintmax_t(WeightTotal),
          uintmax_t(WeightReported), uintmax_t(CountTotal),
          uintmax_t(CountReported));
  size_t Count = 0;
  while (!ValuesHeap->empty()) {
    CountNode::HeapValueType NdPtr = popHeap();
    ++Count;
    fprintf(Out, "%8" PRIuMAX ": ", uintmax_t(Count));
    NdPtr->describe(Out);
  }
}

class AbbreviationsCollector : public CountNodeCollector {
 public:
  AbbreviationsCollector(CountNode::RootPtr Root, IntTypeFormat AbbrevFormat)
      : CountNodeCollector(Root), AbbrevFormat(AbbrevFormat) {}
  void assignAbbreviations(CountNode::PtrVector& Assignment);

 private:
  IntTypeFormat AbbrevFormat;
};

void AbbreviationsCollector::assignAbbreviations(
    CountNode::PtrVector& Assignment) {
  collect(makeFlags(CollectionFlag::All));
  buildHeap();
  while (!ValuesHeap->empty()) {
    CountNode::Ptr Nd = popHeap();
    IntType Index = Assignment.size();
    IntTypeFormats Formats(Index);
    size_t Space = Formats.getByteSize(AbbrevFormat);
    if (Space <= Nd->getWeight()) {
      Nd->setAbbrevIndex(Index);
      Assignment.push_back(Nd);
    }
  }
}

}  // end of anonymous namespace

void IntCompressor::assignInitialAbbreviations() {
  AbbreviationsCollector Collector(getRoot(), AbbrevFormat);
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  CountNode::PtrVector Assignment;
  Collector.assignAbbreviations(Assignment);
}

void IntCompressor::describe(FILE* Out, CollectionFlags Flags) {
  CountNodeCollector Collector(getRoot());
  if (hasTrace())
    Collector.setTrace(getTracePtr());
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.collect(Flags);
  TRACE_BLOCK({ Collector.describe(getTrace().getFile()); });
}

}  // end of namespace intcomp

}  // end of namespace wasm
