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

const char* MethodName[] = {
#define X(tag, name) name,
    BINARY_READER_METHODS_TABLE
#undef X
    "NO_SUCH_METHOD"};

const char* StateName[] = {
#define X(tag, name) name,
    BINARY_READER_STATES_TABLE
#undef X
    "NO_SUCH_STATE"};

}  // end of anonymous namespace

const char* BinaryReader::getName(Method CallMethod) {
  size_t Index = size_t(CallMethod);
  if (Index < size(MethodName))
    return MethodName[Index];
  return "UNDEFINED_METHOD";
}

const char* BinaryReader::getName(State CallState) {
  size_t Index = size_t(CallState);
  if (Index < size(StateName))
    return StateName[Index];
  return "UNDEFINED_STATE";
}

void BinaryReader::CallFrame::describe(FILE* Out) const {
  fprintf(Out, "%s.%s\n", getName(CallMethod), getName(CallState));
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
  if (CurBlockApplyFcn != Method::NO_SUCH_METHOD) {
    fprintf(Out, "*** Apply Fcn ***\n");
    fprintf(Out, "%s\n", getName(CurBlockApplyFcn));
    fprintf(Out, "*****************\n");
  }
}

void BinaryReader::describeNodeStack(FILE* Out) const {
  if (NodeStack.empty())
    return;
  TextWriter Writer;
  fprintf(Out, "*** Node Stack ***\n");
  for (auto* Nd : NodeStack)
    Writer.writeAbbrev(Out, Nd);
  fprintf(Out, "******************\n");
}

void BinaryReader::describeState(FILE* Out) const {
  describeFrameStack(Out);
  describeCounterStack(Out);
  describeCurBlockApplyFcn(Out);
  describeNodeStack(Out);
}

bool BinaryReader::hasEnoughHeadroom() const {
  return isEofFrozen() || (ReadPos.getCurByteAddress() + kResumeHeadroom <=
                           FillPos.getCurByteAddress());
}

void BinaryReader::fail() {
  TRACE_MESSAGE("method failed");
  while (!FrameStack.empty()) {
    returnFromCall();
  }
  Frame.fail();
  delete CurFile;
  delete CurSection;
  CurFile = nullptr;
  CurSection = nullptr;
}

void BinaryReader::fail(const std::string& Message) {
  FILE* Out = getTrace().getFile();
  fprintf(Out, "Error: (method %s) %s\n", getName(Frame.CallMethod),
          Message.c_str());
  fail();
}

void BinaryReader::failBadState() {
  fail(std::string("Bad internal filter s-expression state in method: ") +
       getName(Frame.CallMethod));
}

void BinaryReader::resume() {
#if LOG_CALLSTACK
  TRACE_MESSAGE("enter resume");
#endif
  while (hasEnoughHeadroom()) {
#if LOG_CALLSTACK
    TRACE_BLOCK({ describeState(stderr); });
#endif
    switch (Frame.CallMethod) {
      case Method::Started:
        // If reached, we finished processing the input.
        assert(FrameStack.empty());
        assert(Frame.CallState == State::Enter);
        Frame.CallMethod = Method::Finished;
        if (ReadPos.atEof() && ReadPos.isQueueGood())
          Frame.CallState = State::Succeeded;
        else
          failBadState();
        break;
      case Method::Finished: {
        assert(FrameStack.empty());
        switch (Frame.CallState) {
          case State::Succeeded:
          case State::Failed:
            break;
          default:
            TRACE(string, "State", getName(Frame.CallState));
            TRACE_MESSAGE("Malformed finish state found, Correcting!");
            Frame.CallState = State::Failed;
        }
        return;
      }
      case Method::Block: {
        switch (Frame.CallState) {
          case State::Enter: {
            size_t Size = Reader->readBlockSize(ReadPos);
            TRACE(size_t, "Block size", Size);
            Reader->pushEobAddress(ReadPos, Size);
            Frame.CallState = State::Exit;
            call(CurBlockApplyFcn);
            CurBlockApplyFcn = Method::NO_SUCH_METHOD;
            break;
          }
          case State::Exit:
            ReadPos.popEobAddress();
            returnFromCall();
            break;
          default:
            failBadState();
            break;
        }
        break;
      }
      case Method::File:
        switch (Frame.CallState) {
          case State::Enter: {
            CurFile = Symtab->create<FileNode>();
            uint32_t MagicNumber = Reader->readUint32(ReadPos);
            TRACE(hex_uint32_t, "Casm magic number", MagicNumber);
            uint32_t CasmVersion = Reader->readUint32(ReadPos);
            TRACE(hex_uint32_t, "Casm version number", CasmVersion);
            uint8_t Form = Reader->readUint8(ReadPos);
            WASM_IGNORE(Form);
            TRACE(uint8_t, "Form", Form);
            uint32_t WasmVersion = Reader->readUint32(ReadPos);
            TRACE(hex_uint32_t, "Wasm version number", WasmVersion);
            CurFile->append(Symtab->create<FileVersionNode>(
                Symtab->getCasmMagicDefinition(MagicNumber,
                                               ValueFormat::Hexidecimal),
                Symtab->getCasmVersionDefinition(CasmVersion,
                                                 ValueFormat::Hexidecimal),
                Symtab->getWasmVersionDefinition(WasmVersion,
                                                 ValueFormat::Hexidecimal)));
            uint8_t SectionFlag = Reader->readUint8(ReadPos);
            assert(SectionFlag <= 1);
            if (SectionFlag) {
              Frame.CallState = State::Step2;
              call(Method::Section);
              break;
            }
            Frame.CallState = State::Exit;
            break;
          }
          case State::Step2:
            CurFile->append(NodeStack.back());
            NodeStack.pop_back();
            Frame.CallState = State::Exit;
            break;
          case State::Exit: {
            TRACE_SEXP("File", CurFile);
            SectionSymtab.install(CurFile);
            returnFromCall();
            break;
          }
          default:
            failBadState();
            break;
        }
        break;
      case Method::Name: {
        switch (Frame.CallState) {
          case State::Enter: {
            Name.clear();
            CounterStack.push(Reader->readVaruint32(ReadPos));
            Frame.CallState = State::Loop;
            break;
          }
          case State::Loop: {
            if (Counter-- == 0) {
              CounterStack.pop();
              Frame.CallState = State::Exit;
              break;
            }
            Name.push_back(char(Reader->readUint8(ReadPos)));
            break;
          }
          case State::Exit:
            returnFromCall();
            break;
          default:
            failBadState();
            break;
        }
        break;
      }
      case Method::Node:
        switch (Frame.CallState) {
          case State::Enter: {
            NodeType Opcode = (NodeType)Reader->readUint8(ReadPos);
            TRACE(hex_int32_t, "Opcode", int32_t(Opcode));
            Frame.CallState = State::Exit;
            switch (Opcode) {
              case OpAnd:
                readBinary<AndNode>();
                break;
              case OpBlock:
                readUnary<BlockNode>();
                break;
              case OpBitwiseAnd:
                readBinary<BitwiseAndNode>();
                break;
              case OpBitwiseNegate:
                readUnary<BitwiseNegateNode>();
                break;
              case OpBitwiseOr:
                readBinary<BitwiseOrNode>();
                break;
              case OpBitwiseXor:
                readBinary<BitwiseXorNode>();
                break;
              case OpCallback:
                readUnary<CallbackNode>();
                break;
              case OpCase:
                readBinary<CaseNode>();
                break;
              case OpConvert:
                readTernary<ConvertNode>();
                break;
              case OpDefine:
                readNary<DefineNode>();
                break;
              case OpSymbol: {
                auto* Symbol = SectionSymtab.getIndexSymbol(
                    Reader->readVaruint32(ReadPos));
                NodeStack.push_back(Symbol);
                break;
              }
              case OpError:
                readNullary<ErrorNode>();
                break;
              case OpEval:
                readNary<EvalNode>();
                break;
              case OpFileVersion:
                readTernary<FileVersionNode>();
                break;
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
              case OpLastSymbolIs:
                readUnary<LastSymbolIsNode>();
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
              case OpSet:
                readBinary<SetNode>();
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
              case OpLiteralDef:
                readBinary<LiteralDefNode>();
                break;
              case OpLiteralUse:
                readUnary<LiteralUseNode>();
                break;
              case OpUndefine:
                readUnary<UndefineNode>();
                break;
              case OpVoid:
                readNullary<VoidNode>();
                break;
              case OpWrite:
                readNary<WriteNode>();
                break;
// The following read integer nodes.
#define X(tag, format, defval, mergable, NODE_DECLS)                      \
  case Op##tag: {                                                         \
    Node* Nd;                                                             \
    int FormatIndex = Reader->readUint8(ReadPos);                         \
    if (FormatIndex == 0) {                                               \
      Nd = Symtab->get##tag##Definition();                                \
    } else {                                                              \
      Nd = Symtab->get##tag##Definition(Reader->read##format(ReadPos),    \
                                        getValueFormat(FormatIndex - 1)); \
    }                                                                     \
    TRACE_SEXP(nullptr, Nd);                                              \
    NodeStack.push_back(Nd);                                              \
    break;                                                                \
  }
                AST_OTHER_INTEGERNODE_TABLE
#undef X
// The following read version nodes.
#define X(tag, format, defval, mergable, NODE_DECLS)                      \
  case Op##tag: {                                                         \
    Node* Nd;                                                             \
    int FormatIndex = Reader->readUint8(ReadPos);                         \
    if (FormatIndex == 0) {                                               \
      Nd = Symtab->get##tag##Definition();                                \
    } else {                                                              \
      Nd = Symtab->get##tag##Definition(Reader->read##format(ReadPos),    \
                                        getValueFormat(FormatIndex - 1)); \
    }                                                                     \
    TRACE_SEXP(nullptr, Nd);                                              \
    NodeStack.push_back(Nd);                                              \
    break;                                                                \
  }
                AST_VERSION_INTEGERNODE_TABLE
#undef X
              case NO_SUCH_NODETYPE:
              case OpFile:
              case OpHeader:
              case OpInputHeader:
              case OpOutputHeader:
              case OpSection:
              case OpUnknownSection:
                fprintf(stderr, "Opcode = %u\n", unsigned(Opcode));
                failBadState();
                break;
            }
            break;
          }
          case State::Exit:
            returnFromCall();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::Section:
        switch (Frame.CallState) {
          case State::Enter:
            CurSection = create<SectionNode>();
            CounterStack.push(NodeStack.size());
            CurBlockApplyFcn = Method::SectionBody;
            Frame.CallState = State::Exit;
            call(Method::Block);
            break;
          case State::Exit: {
            size_t StartStackSize = Counter;
            CounterStack.pop();
            size_t StackSize = NodeStack.size();
            if (StackSize < StartStackSize) {
              failBadState();
              break;
            }
            for (size_t i = StartStackSize; i < StackSize; ++i)
              CurSection->append(NodeStack[i]);
            for (size_t i = StartStackSize; i < StackSize; ++i)
              NodeStack.pop_back();
            NodeStack.push_back(CurSection);
            returnFromCall();
            break;
          }
          default:
            failBadState();
            break;
        }
        break;
      case Method::SectionBody:
        switch (Frame.CallState) {
          case State::Enter: {
            assert(CurSection);
            Frame.CallState = State::Loop;
            call(Method::SymbolTable);
            break;
          }
          case State::Loop:
            if (ReadPos.atByteEob()) {
              Frame.CallState = State::Exit;
              break;
            }
            call(Method::Node);
            break;
          case State::Exit:
            returnFromCall();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::SymbolTable:
        switch (Frame.CallState) {
          case State::Enter:
            CounterStack.push(Reader->readVaruint32(ReadPos));
            Frame.CallState = State::Loop;
            break;
          case State::Loop:
            if (Counter-- == 0) {
              CounterStack.pop();
              Frame.CallState = State::Exit;
              break;
            }
            Frame.CallState = State::LoopCont;
            call(Method::Name);
            break;
          case State::LoopCont:
            TRACE(size_t, "index", SectionSymtab.getNumberSymbols());
            TRACE(string, "Symbol", Name);
            SectionSymtab.addSymbol(Name);
            Frame.CallState = State::Loop;
            break;
          case State::Exit:
            returnFromCall();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::NO_SUCH_METHOD:
        failBadState();
        break;
    }
  }
#if LOG_CALLSTACK
  TRACE_BLOCK({ describeState(stderr); });
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
  return Number == CasmBinaryMagic;
}

BinaryReader::BinaryReader(std::shared_ptr<decode::Queue> Input,
                           std::shared_ptr<SymbolTable> Symtab)
    : Reader(std::make_shared<ByteReadStream>()),
      ReadPos(StreamType::Byte, Input),
      FillPos(StreamType::Byte, Input),
      Input(Input),
      Symtab(Symtab),
      SectionSymtab(Symtab),
      CurFile(nullptr),
      CurBlockApplyFcn(Method::NO_SUCH_METHOD),
      FrameStack(Frame),
      Counter(0),
      CounterStack(Counter) {
}

void BinaryReader::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (!Trace)
    return;
  Trace->addContext(ReadPos.getTraceContext());
}

TraceClassSexp& BinaryReader::getTrace() const {
  if (!Trace) {
    const_cast<BinaryReader*>(this)
        ->setTrace(std::make_shared<TraceClassSexp>("BinaryReader"));
  }
  return *Trace;
}

template <class T>
void BinaryReader::readNullary() {
  auto* Node = Symtab->create<T>();
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnary() {
  if (NodeStack.size() < 1) {
    failBadState();
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
    failBadState();
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
    failBadState();
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
    failBadState();
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
  TRACE_BLOCK({ describeState(stderr); });
  return getFile();
}

void BinaryReader::startReadingFile() {
  Frame.init();
  call(Method::File);
}

}  // end of namespace filt

}  // end of namespace wasm
