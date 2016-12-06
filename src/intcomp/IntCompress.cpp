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

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab)
    : Input(Input),
      Output(Output),
      Symtab(Symtab),
      CountCutoff(0),
      WeightCutoff(0),
      LengthLimit(1),
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

class RemoveFrame {
 public:
  enum class State { Enter, Visiting, Exit };
  RemoveFrame(CountNode::RootPtr Root, size_t FirstKid, size_t LastKid)
      : FirstKid(FirstKid),
        LastKid(LastKid),
        CurKid(FirstKid),
        CurState(State::Enter),
        Root(Root) {
    assert(Root);
  }
  RemoveFrame(CountNode::RootPtr Root,
              CountNode::IntPtr Nd,
              size_t FirstKid,
              size_t LastKid)
      : FirstKid(FirstKid),
        LastKid(LastKid),
        CurKid(FirstKid),
        CurState(State::Enter),
        Root(Root),
        Nd(Nd) {
    assert(Root);
    assert(Nd);
  }
  size_t FirstKid;
  size_t LastKid;
  size_t CurKid;
  State CurState;
  CountNode::RootPtr getRoot() const { return Root; }
  CountNode::IntPtr getIntNode() const { return Nd; }
  CountNode::WithSuccsPtr getBase() const {
    if (Nd)
      return Nd;
    return Root;
  }
  std::vector<CountNode::IntPtr> RemoveSet;
  void describe(FILE* Out) const;

 private:
  CountNode::RootPtr Root;
  CountNode::IntPtr Nd;
};

void RemoveFrame::describe(FILE* Out) const {
  fputs("<Frame ", Out);
  getBase()->describe(Out);
  fprintf(stderr, "  %" PRIuMAX "..%" PRIuMAX " [%" PRIuMAX ":%u]\n",
          uintmax_t(FirstKid), uintmax_t(LastKid), uintmax_t(CurKid),
          unsigned(CurState));
  for (auto Nd : RemoveSet) {
    fputs("  ", Out);
    Nd->describe(Out);
  }
  fputs(">\n", Out);
}

}  // end of anonymous namespace

void IntCompressor::removeSmallUsageCounts() {
  // NOTE: The main purpose of this method is to shrink the size of
  // the trie to (a) recover memory and (b) make remaining analysis
  // faster.  It does this by removing int count nodes that are not
  // not useful (See case RemoveFrame::State::Exit for details).
  std::vector<CountNode::IntPtr> ToVisit;
  std::vector<RemoveFrame> FrameStack;
  for (CountNode::SuccMapIterator Iter = Root->getSuccBegin(),
                                  End = Root->getSuccEnd();
       Iter != End; ++Iter) {
    ToVisit.push_back(Iter->second);
  }
  FrameStack.push_back(RemoveFrame(Root, 0, ToVisit.size()));
  while (!FrameStack.empty()) {
    RemoveFrame& Frame(FrameStack.back());
    switch (Frame.CurState) {
      case RemoveFrame::State::Enter: {
        if (Frame.CurKid >= Frame.LastKid) {
          Frame.CurState = RemoveFrame::State::Visiting;
          continue;
        }
        CountNode::IntPtr CurNd = ToVisit[Frame.CurKid++];
        // If reached, simulate recursive calls on CurNod;
        size_t FirstKid = ToVisit.size();
        for (CountNode::SuccMapIterator Iter = CurNd->getSuccBegin(),
                                        End = CurNd->getSuccEnd();
             Iter != End; ++Iter) {
          ToVisit.push_back(Iter->second);
        }
        FrameStack.push_back(
            RemoveFrame(Root, CurNd, FirstKid, ToVisit.size()));
        break;
      }
      case RemoveFrame::State::Visiting: {
        Frame.CurState = RemoveFrame::State::Exit;
        while (!Frame.RemoveSet.empty()) {
          CountNode::IntPtr Nd = Frame.RemoveSet.back();
          Frame.RemoveSet.pop_back();
          if (isa<SingletonCountNode>(*Nd))
            Root->getDefaultSingle()->increment(Nd->getCount());
          Frame.getBase()->eraseSucc(Nd->getValue());
        }
        break;
      }
      case RemoveFrame::State::Exit: {
        // Pop frame. Keep hold of node from popped frame so that it
        // can be checked for usefulness.
        CountNode::IntPtr Nd = Frame.getIntNode();
        while (ToVisit.size() > Frame.FirstKid)
          ToVisit.pop_back();
        FrameStack.pop_back();
        if (!Nd)
          break;
        if (!FrameStack.empty() &&
            !(Nd->hasSuccessors() ||
              (Nd->getWeight() >= CountCutoff && Nd->getCount() > 1))) {
          FrameStack.back().RemoveSet.push_back(Nd);
        }
        break;
      }
    }
  }
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
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // trie.
  if (!compressUpToSize(1, TraceParsing && !TraceFirstPassOnly))
    return;
  if (Level == AllDetail)
    describe(stderr, makeFlags(CollectionFlag::TopLevel));
  removeSmallUsageCounts();
  if (LengthLimit > 1) {
    if (!compressUpToSize(LengthLimit, TraceParsing && !TraceFirstPassOnly))
      return;
    removeSmallUsageCounts();
  }
  if (Level == AllDetail)
    describe(stderr, makeFlags(CollectionFlag::IntPaths));
  assignInitialAbbreviations();
  if (Level != DetailLevel::NoDetail)
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
  void assignInitialAbbreviations();

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

void CountNodeCollector::assignInitialAbbreviations() {
  collect(makeFlags(CollectionFlag::All));
  buildHeap();
  IntType AbbrevIndex = 0;
  while (!ValuesHeap->empty()) {
    CountNode::HeapValueType NdPtr = popHeap();
    NdPtr->setAbbrevIndex(AbbrevIndex++);
    // TODO(karlschimpf): Fix related nodes (prefix/suffix).
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

}  // end of anonymous namespace

void IntCompressor::assignInitialAbbreviations() {
  CountNodeCollector Collector(getRoot());
  Collector.CountCutoff = CountCutoff;
  Collector.WeightCutoff = WeightCutoff;
  Collector.assignInitialAbbreviations();
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
