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

// implements a reader to extract filter sections.

#include "binary/BinaryReader.h"
#include "interp/ByteReadStream.h"
#include "sexp/TextWriter.h"

#include <cstdio>

// Show call stack on enter/exit of resume().
#define LOG_CALLSTACK 0

using namespace wasm::decode;
using namespace wasm::utils;

namespace wasm {

namespace filt {

namespace {
// Note: Headroom is used to guarantee that we have enough space to
// read any sexpression node.
static constexpr size_t kResumeHeadroom = 100;

const char* RunMethodName[] = {
#define X(tag, name) name,
    BINARY_READER_METHODS_TABLE
#undef X
    "NO_SUCH_METHOD"
};

const char* RunStateName[] = {
#define X(tag, name) name,
    BINARY_READER_STATES_TABLE
#undef X
    "NO_SUCH_STATE"
};

} // end of anonymous namespace

const char* BinaryReader::getName(RunMethod Method) {
  size_t Index = size_t(Method);
  if (Index < size(RunMethodName))
    return RunMethodName[Index];
  return "UNDEFINED_METHOD";
}

const char* BinaryReader::getName(RunState State) {
  size_t Index = size_t(State);
  if (Index < size(RunStateName))
    return RunStateName[Index];
  return "UNDEFINED_STATE";
}

void BinaryReader::CallFrame::describe(FILE* Out) const {
  fprintf(Out, "%s.%s\n", getName(Method), getName(State));
}

void BinaryReader::describeFrameStack(FILE* Out) const {
  fprintf(Out, "*** Frame Stack ***\n");
  for (auto& Frame : FrameStack) {
    fputs("  ", Out);
    Frame.describe(Out);
  }
  fprintf(Out, "*******************\n");
}

void BinaryReader::describeCounterStack(FILE* Out) const {
  if (!CounterStack.empty()) {
    fprintf(Out, "*** Counter Stack ***\n");
    for (auto& Value : CounterStack.iterRange(1)) {
      fprintf(Out, "%" PRIuMAX "\n", uintmax_t(Value));
    }
    fprintf(Out, "*********************\n");
  }
}

void BinaryReader::describeCurBlockApplyFcn(FILE* Out) const {
  if (CurBlockApplyFcn != RunMethod::NO_SUCH_METHOD) {
    fprintf(Out, "*** Apply Fcn ***\n");
    fprintf(Out, "%s\n", getName(CurBlockApplyFcn));
    fprintf(Out, "*****************\n");
  }
}

void BinaryReader::describeNodeStack(FILE* Out,
                                     TextWriter* Writer) const {
  if (NodeStack.empty())
    return;
  fprintf(Out, "*** Node Stack ***\n");
  for (auto* Nd : NodeStack)
    Writer->writeAbbrev(Out, Nd);
  fprintf(Out, "******************\n");
}

void BinaryReader::describeRunState(FILE* Out) const {
  describeFrameStack(Out);
  describeCounterStack(Out);
  describeCurBlockApplyFcn(Out);
  describeNodeStack(Out, getTextWriter());
}

bool BinaryReader::hasEnoughHeadroom() const {
  return isEofFrozen() ||
      (ReadPos.getCurByteAddress() + kResumeHeadroom
       <= FillPos.getCurByteAddress());
}

void BinaryReader::fail() {
  TRACE_MESSAGE("method failed");
  while (!FrameStack.empty()) {
    returnFromCall();
  }
  Frame.fail();
}

void BinaryReader::fail(const std::string& Message) {
  FILE* Out = getTrace().getFile();
  fprintf(Out, "Error: (method %s) %s\n", getName(Frame.Method),
          Message.c_str());
  fail();
}

void BinaryReader::resume() {
#if LOG_CALLSTACK
  TRACE_MESSAGE("enter resume");
#endif
  while (hasEnoughHeadroom()) {
#if LOG_CALLSTACK
  TRACE_BLOCK({ describeRunState(stderr); });
#endif
    switch (Frame.Method) {
      case RunMethod::Started:
        // If reached, we finished processing the input.
        assert(FrameStack.empty());
        Frame.Method = RunMethod::Finished;
        if (ReadPos.atEof() && ReadPos.isQueueGood())
          Frame.State = RunState::Succeeded;
        else
          fail("Malformed input in decompression algorithms file");
        break;
      case RunMethod::Finished: {
        assert(FrameStack.empty());
        switch (Frame.State) {
          case RunState::Succeeded:
          case RunState::Failed:
            break;
          default:
            TRACE(string, "State", getName(Frame.State));
            TRACE_MESSAGE("Malformed finish state found, Correcting!");
            Frame.State = RunState::Failed;
        }
        return;
      }
      case RunMethod::Block: {
        switch (Frame.State) {
          case RunState::Enter: {
            size_t Size = Reader->readBlockSize(ReadPos);
            TRACE(size_t, "Block size", Size);
            Reader->pushEobAddress(ReadPos, Size);
            Frame.State = RunState::Exit;
            call(CurBlockApplyFcn);
            CurBlockApplyFcn = RunMethod::NO_SUCH_METHOD;
            break;
          }
          case RunState::Exit:
            ReadPos.popEobAddress();
            CurBlockApplyFcn = RunMethod::NO_SUCH_METHOD;
            returnFromCall();
            break;
          default:
            fatal("resume reading block not implemented");
            break;
        }
        break;
      }
      case RunMethod::File:
        switch (Frame.State) {
          case RunState::Enter: {
            CurFile = readHeader();
            Frame.State = RunState::Loop;
            call(RunMethod::Section);
            break;
          }
          case RunState::Loop: {
            CurFile->append(CurSection);
            TRACE_SEXP("CurSection", CurSection);
            CurSection = nullptr;
            if (ReadPos.atEof()) {
              Frame.State = RunState::Exit;
              break;
            }
            call(RunMethod::Section);
            break;
          }
          case RunState::Exit: {
            SectionSymtab.install(CurFile);
            returnFromCall();
            break;
          }
          default:
            fatal("resume reading file not implemented");
            break;
        }
        break;
      case RunMethod::Name: {
        switch (Frame.State) {
          case RunState::Enter: {
            Name.clear();
            CounterStack.push();
            Counter = Reader->readVaruint32(ReadPos);
            Frame.State = RunState::Loop;
            break;
          }
          case RunState::Loop: {
            if (Counter-- == 0) {
              CounterStack.pop();
              Frame.State = RunState::Exit;
              break;
            }
            Name.push_back(char(Reader->readUint8(ReadPos)));
            break;
          }
          case RunState::Exit:
            returnFromCall();
            break;
          default:
            fatal("resume reading name not implemented");
            break;
        }
        break;
      }
      case RunMethod::Node:
        switch (Frame.State) {
          case RunState::Enter: {
            NodeType Opcode = (NodeType)Reader->readUint8(ReadPos);
            Frame.State = RunState::Exit;
            switch (Opcode) {
              case OpAnd:
                readBinary<AndNode>();
                break;
              case OpBlock:
                readUnary<BlockNode>();
                break;
              case OpCase:
                readBinary<CaseNode>();
                break;
              case OpConvert:
                readTernary<ConvertNode>();
                break;
              case OpDefine: {
                auto* Symbol =
                    SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
                auto* Body = NodeStack.back();
                NodeStack.pop_back();
                auto* Params = NodeStack.back();
                NodeStack.pop_back();
                auto* Node = Symtab->create<DefineNode>(Symbol, Params, Body);
                TRACE_SEXP(nullptr, Node);
                NodeStack.push_back(Node);
                break;
              }
              case OpError:
                readNullary<ErrorNode>();
                break;
              case OpEval: {
                auto* Node = Symtab->create<EvalNode>();
                auto *Sym =
                    SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
                Node->append(Sym);
                uint32_t NumParams = Reader->readVaruint32(ReadPos);
                size_t StackSize = NodeStack.size();
                if (StackSize < NumParams) {
                  fail("Can't find arguments for eval s-expression");
                  break;
                }
                for (size_t i = StackSize - NumParams; i < StackSize; ++i)
                  Node->append(NodeStack[i]);
                for (uint32_t i = 0; i < NumParams; ++i)
                  NodeStack.pop_back();
                TRACE_SEXP(nullptr, Node);
                NodeStack.push_back(Node);
                break;
              }
              case OpFilter:
                readNary<FilterNode>();
                break;
              case OpIfThen:
                readBinary<IfThenNode>();
                break;
              case OpIfThenElse:
                readTernary<IfThenElseNode>();
                break;
              case OpLastRead:
                readNullary<LastReadNode>();
                break;
              case OpLoop:
                readBinary<LoopNode>();
                break;
              case OpLoopUnbounded:
                readUnary<LoopUnboundedNode>();
                break;
              case OpMap:
                readNary<MapNode>();
                break;
              case OpNot:
                readUnary<NotNode>();
                break;
              case OpOpcode:
                readNary<OpcodeNode>();
                break;
              case OpOr:
                readBinary<OrNode>();
                break;
              case OpPeek:
                readUnary<PeekNode>();
                break;
              case OpRead:
                readUnary<ReadNode>();
                break;
             case OpRename:
                readBinary<RenameNode>();
                break;
              case OpSequence:
                readNary<SequenceNode>();
                break;
              case OpStream: {
                uint8_t Encoding = Reader->readUint8(ReadPos);
                StreamKind StrmKind;
                StreamType StrmType;
                StreamNode::decode(Encoding, StrmKind, StrmType);
                auto* Node = Symtab->create<StreamNode>(StrmKind, StrmType);
                TRACE_SEXP(nullptr, Node);
                NodeStack.push_back(Node);
                break;
              }
              case OpSwitch:
                readNary<SwitchNode>();
                break;
              case OpUndefine:
                readUnary<UndefineNode>();
                break;
              case OpVoid:
                readNullary<VoidNode>();
                break;
              case OpWrite:
                readBinary<WriteNode>();
                break;
              // The following read integer nodes.
#define X(tag, format, defval, mergable, NODE_DECLS)                           \
              case Op##tag: {                                                  \
                Node* Nd;                                                      \
                if (Reader->readUint8(ReadPos)) {                              \
                  Nd = Symtab->get##tag##Definition();                         \
                } else {                                                       \
                  Nd = Symtab->get##tag##Definition(                           \
                      Reader->read##format(ReadPos), ValueFormat::Decimal);    \
                }                                                              \
                TRACE_SEXP(nullptr, Nd);                                       \
                NodeStack.push_back(Nd);                                       \
                break;                                                         \
              }
              AST_INTEGERNODE_TABLE
#undef X
              case NO_SUCH_NODETYPE:
              case OpFile:
              case OpSection:
              case OpSymbol:
              case OpUnknownSection:
                fail("Illegal opcode given to readNode");
                break;
            }
            break;
          }
          case RunState::Exit:
            returnFromCall();
            break;
          default:
            fatal("resume reading name not implemented");
            break;
        }
        break;
      case RunMethod::Section:
        switch (Frame.State) {
          case RunState::Enter: {
            Frame.State = RunState::Setup;
            call(RunMethod::Name);
            break;
          }
          case RunState::Setup: {
            CurSection = create<SectionNode>();
            CurSection->append(create<SymbolNode>(Name));
            // Save StartStackSize for exit.
            CounterStack.push();
            Counter = NodeStack.size();
            CurBlockApplyFcn = RunMethod::SectionBody;
            Frame.State = RunState::Exit;
            call(RunMethod::Block);
            break;
          }
          case RunState::Exit: {
            size_t StartStackSize = Counter;
            CounterStack.pop();
            size_t StackSize = NodeStack.size();
            if (StackSize < StartStackSize)
              fatal("Malformed section: " + Name);
            for (size_t i = StartStackSize; i < StackSize; ++i)
              CurSection->append(NodeStack[i]);
            for (size_t i = StartStackSize; i < StackSize; ++i)
              NodeStack.pop_back();
            returnFromCall();
            break;
          }
          default:
            fatal("resume reading section not implemented");
            break;
        }
        break;
      case RunMethod::SectionBody:
        switch (Frame.State) {
          case RunState::Enter: {
            SymbolNode *Sym = CurSection->getSymbol();
            assert(Sym);
            if (Sym->getStringName() != "filter")
              fatal("Handling non-filter sections not implemented!");
            Frame.State = RunState::Loop;
            call(RunMethod::SymbolTable);
            break;
          }
          case RunState::Loop:
            if (ReadPos.atByteEob()) {
              Frame.State = RunState::Exit;
              break;
            }
            call(RunMethod::Node);
            break;
          case RunState::Exit:
            returnFromCall();
            break;
          default:
            fatal("resume section body not implemented");
            break;
        }
        break;
      case RunMethod::SymbolTable:
        switch (Frame.State) {
          case RunState::Enter: {
            SectionSymtab.clear();
            CounterStack.push();
            Counter = Reader->readVaruint32(ReadPos);
            Frame.State = RunState::Loop;
            break;
          }
          case RunState::Loop: {
            if (Counter-- == 0) {
              CounterStack.pop();
              Frame.State = RunState::Exit;
              break;
            }
            Frame.State = RunState::LoopCont;
            call(RunMethod::Name);
            break;
          }
          case RunState::LoopCont:
            TRACE(size_t, "index", SectionSymtab.getNumberSymbols());
            TRACE(string, "Symbol", Name);
            SectionSymtab.addSymbol(Name);
            Frame.State = RunState::Loop;
            break;
          case RunState::Exit:
            returnFromCall();
            break;
          default:
            fatal("resume reading symbol table not implemented");
            break;
        }
        break;
      case RunMethod::NO_SUCH_METHOD:
        fatal("resume on unknown method!");
        break;
    }
  }
#if LOG_CALLSTACK
  TRACE_BLOCK({ describeRunState(stderr); });
  TRACE_MESSAGE("exit resume");
#endif
  return;
}

bool BinaryReader::isBinary(const char* Filename) {
  FILE* File = fopen(Filename, "r");
  if (File == nullptr)
    return false;
  // Read first 4 bytes and generate magic number.
  uint32_t Number = 0;
  uint32_t Shift = 0;
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    uint8_t Byte = fgetc(File);
    Number |= (Byte << Shift);
    Shift += CHAR_BIT;
  }
  fclose(File);
  return Number == WasmBinaryMagic;
}

BinaryReader::BinaryReader(std::shared_ptr<decode::Queue> Input,
                           std::shared_ptr<SymbolTable> Symtab)
    : Reader(std::make_shared<ByteReadStream>()),
      ReadPos(StreamType::Byte, Input),
      FillPos(StreamType::Byte, Input),
      Input(Input),
      Symtab(Symtab),
      SectionSymtab(Symtab),
      Trace(&ReadPos, "BinaryReader"),
      CurFile(nullptr),
      CurBlockApplyFcn(RunMethod::NO_SUCH_METHOD),
      FrameStack(Frame),
      Counter(0),
      CounterStack(Counter) {}

template <class T>
void BinaryReader::readNullary() {
  auto* Node = Symtab->create<T>();
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnary() {
  if (NodeStack.size() < 1) {
    fail("Can't find argument for unary s-expression");
    return;
  }
  Node* Arg = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Arg);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUint8() {
  auto* Node = Symtab->create<T>(Reader->readUint8(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVarint32() {
  auto* Node = Symtab->create<T>(Reader->readVarint32(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVarint64() {
  auto* Node = Symtab->create<T>(Reader->readVarint64(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVaruint32() {
  auto* Node = Symtab->create<T>(Reader->readVaruint32(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVaruint64() {
  auto* Node = Symtab->create<T>(Reader->readVarint64(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readBinary() {
  if (NodeStack.size() < 2) {
    fail("Can't find arguments for binary s-expression");
    return;
  }
  Node* Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Arg1, Arg2);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readBinarySymbol() {
  auto* Symbol = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto* Body = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Symbol, Body);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readTernary() {
  if (NodeStack.size() < 3) {
    fail("Can't find argument or ternary s-expression");
    return;
  }
  Node* Arg3 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Arg1, Arg2, Arg3);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readNary() {
  uint32_t NumKids = Reader->readVaruint32(ReadPos);
  size_t StackSize = NodeStack.size();
  if (StackSize < NumKids) {
    fail("Can't find arguments for n-ary s-expression");
    return;
  }
  auto* Node = Symtab->create<T>();
  for (size_t i = StackSize - NumKids; i < StackSize; ++i)
    Node->append(NodeStack[i]);
  for (uint32_t i = 0; i < NumKids; ++i)
    NodeStack.pop_back();
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

FileNode* BinaryReader::readHeader() {
  TRACE_METHOD("readHeader");
  MagicNumber = Reader->readUint32(ReadPos);
  TRACE(uint32_t, "MagicNumber", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to read, did not find WASM binary magic number");
  Version = Reader->readUint32(ReadPos);
  TRACE(uint32_t, "Version", Version);
  auto *File = Symtab->create<FileNode>();
  return File;
}

void BinaryReader::readBackFilled() {
  decode::ReadCursor FillPos(ReadPos);
  while (!isFinished()) {
    if (!FillPos.atEof())
      FillPos.advance(Page::Size);
    resume();
  }
}

FileNode* BinaryReader::readFile() {
  TRACE_METHOD("toplevel.readFile");
  startReadingFile();
  readBackFilled();
  TRACE_BLOCK({ describeRunState(stderr); });
  return getFile();
}

SectionNode* BinaryReader::readSection() {
  TRACE_METHOD("toplevel.readSection");
  startReadingSection();
  readBackFilled();
  return getSection();
}

void BinaryReader::startReadingSection() {
  Frame.init();
  call(RunMethod::Section);
}

void BinaryReader::startReadingFile() {
  Frame.init();
  call(RunMethod::File);
}

}  // end of namespace filt

}  // end of namespace wasm
