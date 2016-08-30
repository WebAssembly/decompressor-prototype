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

// Implements the interpreter for filter s-expressions.

#include "interp/ByteReadStream.h"
#include "interp/ByteWriteStream.h"
#include "interp/Interpreter.h"
#include "sexp/TextWriter.h"
#include "utils/backwards_iterator.h"

#include <iostream>

// By default, methods runMethods() and readBackFilled() are not traced,
// since they are the glue between a push and pull models. Rather, they
// conceptually mimic the natural call structure. If you want to trace
// runMethods() and readBackFilled() as well, change this flag to 1.
#define LOG_RUNMETHODS 0
// The following tracks runMetthods() and readBackFilled(), which run
// interpreter methods with tracing showing equivalent non-push inter
// The following turn on logging sections, functions in the decompression
// algorithm.
#define LOG_SECTIONS 0
#define LOG_FUNCTIONS 0
// The following logs lookahead on each call to eval.
#define LOG_EVAL_LOOKAHEAD 0

// The following two defines allows turning on tracing for the nth (zero based)
// function.
#define LOG_NUMBERED_BLOCK 0
#define LOG_FUNCTION_NUMBER 0

// The following shows stack contents on each iteration of runMethods();
#define LOG_CALLSTACKS 1

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

namespace {

static constexpr uint32_t MaxExpectedSectionNameSize = 32;

static constexpr size_t DefaultStackSize = 256;

// Note: Headroom is used to guarantee that we have enough space to
// read any sexpression node.
static constexpr size_t kResumeHeadroom = 100;

#define X(tag, name) constexpr const char* Method##tag##Name = name;
INTERPRETER_METHODS_TABLE
#undef X

const char* MethodName[] = {
#define X(tag, name) Method##tag##Name,
    INTERPRETER_METHODS_TABLE
#undef X
    "NO_SUCH_METHOD"};

const char* StateName[] = {
#define X(tag, name) name,
    INTERPRETER_STATES_TABLE
#undef X
    "NO_SUCH_STATE"};

}  // end of anonymous namespace

const char* Interpreter::getName(Method M) {
  return MethodName[int(M)];
}

const char* Interpreter::getName(State S) {
  return StateName[int(S)];
}

Interpreter::Interpreter(std::shared_ptr<Queue> Input,
                         std::shared_ptr<Queue> Output,
                         std::shared_ptr<SymbolTable> Symtab)
    : ReadPos(StreamType::Byte, Input),
      Reader(std::make_shared<ByteReadStream>()),
      WritePos(StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      Symtab(Symtab),
      LastReadValue(0),
      DispatchedMethod(Method::NO_SUCH_METHOD),
      MinimizeBlockSize(false),
      Trace(ReadPos, WritePos, "InterpSexp"),
      FrameStack(Frame),
      CallingEvalStack(CallingEval),
      WriteValueStack(WriteValue),
      PeekPosStack(PeekPos),
      BlockStartStack(BlockStart),
      LoopCounter(0),
      LoopCounterStack(LoopCounter),
      OpcodeLocalsStack(OpcodeLocals) {
  DefaultFormat = Symtab->create<Varuint64NoArgsNode>();
  CurSectionName.reserve(MaxExpectedSectionNameSize);
  FrameStack.reserve(DefaultStackSize);
  WriteValueStack.reserve(DefaultStackSize);
  CallingEvalStack.reserve(DefaultStackSize);
}

void Interpreter::CallFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "%s.%s = %" PRIuMAX ": ", MethodName[int(CallMethod)],
          StateName[int(CallState)], uintmax_t(ReturnValue));
  Writer->writeAbbrev(File, Nd);
}

void Interpreter::EvalFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "cc = %" PRIuMAX ": ", uintmax_t(CallingEvalIndex));
  Writer->writeAbbrev(File, Caller);
}

void Interpreter::OpcodeLocalsFrame::describe(FILE* File,
                                              TextWriter* Writer) const {
  fprintf(File, "OpcodeFrame <%" PRIuMAX ",%" PRIuMAX "> ", uintmax_t(SelShift),
          uintmax_t(CaseMask));
  if (Writer && Case != nullptr)
    Writer->writeAbbrev(File, Case);
  else
    fprintf(File, "%p\n", (void*)Case);
}

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

void Interpreter::describeFrameStack(FILE* File) {
  fprintf(File, "*** Frame Stack ***\n");
  for (auto& Frame : FrameStack)
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "*******************\n");
}

void Interpreter::describeWriteValueStack(FILE* File) {
  fprintf(File, "*** WriteValue Stack ***\n");
  for (auto& Value : WriteValueStack.iterRange(1))
    fprintf(File, "%" PRIuMAX "\n", uintmax_t(Value));
  fprintf(File, "************************\n");
}

void Interpreter::describeCallingEvalStack(FILE* File) {
  fprintf(File, "*** Eval Call Stack ****\n");
  for (const auto& Frame : CallingEvalStack.iterRange(1))
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "************************\n");
}

void Interpreter::describePeekPosStack(FILE* File) {
  fprintf(File, "*** Peek Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : PeekPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "**********************\n");
}

void Interpreter::describeLoopCounterStack(FILE* File) {
  fprintf(File, "*** Loop Counter Stack ***\n");
  for (const auto& Count : LoopCounterStack.iterRange(1))
    fprintf(File, "%" PRIxMAX "\n", uintmax_t(Count));
  fprintf(File, "**************************\n");
}

void Interpreter::describeBlockStartStack(FILE* File) {
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& Pos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void Interpreter::describeOpcodeLocalStack(FILE* File) {
  fprintf(File, "*** Opcode Stack ***\n");
  for (auto& Frame : OpcodeLocalsStack.iterRange(1))
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "********************\n");
}

void Interpreter::describeAllNonemptyStacks(FILE* File) {
  describeFrameStack(File);
  if (!WriteValueStack.empty())
    describeWriteValueStack(File);
  if (!CallingEvalStack.empty())
    describeCallingEvalStack(File);
  if (!PeekPosStack.empty())
    describePeekPosStack(File);
  if (!LoopCounterStack.empty())
    describeLoopCounterStack(File);
  if (!BlockStartStack.empty())
    describeBlockStartStack(File);
  if (!OpcodeLocalsStack.empty())
    describeOpcodeLocalStack(File);
}

bool Interpreter::hasEnoughHeadroom() const {
  return ReadPos.isEofFrozen() ||
         (ReadPos.getCurByteAddress() + kResumeHeadroom <=
          ReadPos.getCurByteAddress());
}

IntType Interpreter::eval(const Node* Nd) {
  TRACE_METHOD("oldeval");
  // TODO(karlschimpf): Remove this when fully transitioned.
  if (!FrameStack.empty())
    fatal("Nested calls no longer allowed!");
  callTopLevel(Method::Eval, Nd);
  readBackFilled();
  TRACE(IntType, "returns", Frame.ReturnValue);
  return Frame.ReturnValue;
}

IntType Interpreter::read(const Node* Nd) {
  TRACE_METHOD("oldread");
  // TODO(karlschimpf): Remove this when fully transitioned.
  if (!FrameStack.empty())
    fatal("Nested calls no longer allowed!");
  callTopLevel(Method::Read, Nd);
  readBackFilled();
  TRACE(IntType, "returns", Frame.ReturnValue);
  return Frame.ReturnValue;
}

void Interpreter::callTopLevel(Method Method, const filt::Node* Nd) {
  // First verify stacks cleared.
  Frame.reset();
  FrameStack.clear();
  WriteValue = 0;
  WriteValueStack.clear();
  PeekPos = ReadCursor();
  PeekPosStack.clear();
  LoopCounter = 0;
  LoopCounterStack.clear();
  BlockStart = WriteCursor();
  BlockStartStack.clear();
  OpcodeLocals.reset();
  OpcodeLocalsStack.clear();
  call(Method, Nd);
}

void Interpreter::readBackFilled() {
#if LOG_RUNMETHODS
  TRACE_METHOD("readBackFilled");
#endif
  if (FrameStack.empty())
    // Clear from previous run.
    Frame.reset();
  ReadCursor FillPos(ReadPos);
  while (needsMoreInput() && !errorsFound()) {
    while (!hasEnoughHeadroom()) {
      FillPos.advance(Page::Size);
    }
    runMethods();
  }
  // TODO(karlschimpf): Remove this when transitioned to runMethods()
  // medel complete.
  if (errorsFound())
    fatal("Unable to continue, errors found");
}

void Interpreter::fail() {
  TRACE_MESSAGE("method failed");
  while (!FrameStack.empty()) {
    TraceExitFrame();
    popAndReturn();
  }
  Frame.fail();
}

void Interpreter::fail(const char* Message) {
  FILE* Out = getTrace().getFile();
  fprintf(Out, "Error: ");
  getTrace().getTextWriter()->writeAbbrev(Out, Frame.Nd);
  fprintf(Out, "Error: %s\n: ", Message);
  fail();
}

void Interpreter::runMethods() {
#if LOG_RUNMETHODS
  TRACE_ENTER("runMethods");
  TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
#endif
  while (hasEnoughHeadroom() && !errorsFound()) {
#if LOG_CALLSTACKS
    TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
#endif
    switch (Frame.CallMethod) {
      case Method::NO_SUCH_METHOD:
        assert(false);
        fail("Unknown internal method call!");
        break;
      case Method::Eval:
#if LOG_EVAL_LOOKAHEAD
        if (Frame.State == State::Enter) {
          TRACE_BLOCK({
            decode::ReadCursor Lookahead(ReadPos);
            fprintf(TRACE.indent(), "Lookahead:");
            for (int i = 0; i < 10; ++i) {
              if (!Lookahead.atByteEob())
                fprintf(TRACE.getFile(), " %x", Lookahead.readByte());
            }
            fprintf(TRACE.getFile(), " ");
            fprintf(ReadPos.describe(TRACE.getFile(), true), "\n");
          });
        }
#endif
        switch (Frame.Nd->getType()) {
          case OpBlockEndNoArgs:
          case OpConvert:
          case OpFilter:  // Method::Eval
            fail("Eval not implemented yet for construct");
            break;
          case NO_SUCH_NODETYPE:
          case OpFile:
          case OpRename:
          case OpSymbol:
          case OpSection:
          case OpUndefine:
          case OpUnknownSection:
          case OpVersion:  // Method::Eval
            fail("Eval not allowed on construct!");
            break;
          case OpError:  // Method::Eval
            TraceEnterFrame();
            fail("Algorithm error!");
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const:
          case OpLastRead:
          case OpPeek:
          case OpRead:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpUint8NoArgs:
          case OpUint8OneArg:
          case OpUint32NoArgs:
          case OpUint32OneArg:
          case OpUint64NoArgs:
          case OpUint64OneArg:
          case OpVarint32NoArgs:
          case OpVarint32OneArg:
          case OpVarint64NoArgs:
          case OpVarint64OneArg:
          case OpVaruint32NoArgs:
          case OpVaruint32OneArg:
          case OpVaruint64NoArgs:
          case OpVaruint64OneArg:
          case OpMap:
          case OpOpcode:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Read, Frame.Nd);
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                callWrite(Method::Write, Frame.Nd, Frame.ReturnValue);
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpWrite:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Read, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                callWrite(Method::Write, Frame.Nd->getKid(1),
                          Frame.ReturnValue);
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpStream: {  // Method::Eval
            TraceEnterFrame();
            const auto* Stream = cast<StreamNode>(Frame.Nd);
            switch (Stream->getStreamKind()) {
              case StreamKind::Input:
                switch (Stream->getStreamType()) {
                  case StreamType::Byte:
                    popAndReturn(int(isa<ByteReadStream>(Reader.get())));
                    TraceExitFrame();
                    break;
                  case StreamType::Bit:
                  case StreamType::Int:
                  case StreamType::Ast:
                    Trace.errorSexp("Stream check: ", Frame.Nd);
                    fail("Stream check not implemented!");
                    break;
                }
                break;
              case StreamKind::Output:
                switch (Stream->getStreamType()) {
                  case StreamType::Byte:
                    popAndReturn(int(isa<ByteReadStream>(Writer.get())));
                    TraceExitFrame();
                    break;
                  case StreamType::Bit:
                  case StreamType::Int:
                  case StreamType::Ast:
                    Trace.errorSexp("Stream check: ", Frame.Nd);
                    fail("Stream check not implemented!");
                    break;
                }
                break;
            }
            break;
          }
          case OpNot:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue != 0);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpAnd:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpOr:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue == 0)
                  call(Method::Eval, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpSequence:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                LoopCounterStack.push();
                LoopCounter = 0;
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (LoopCounter >= size_t(Frame.Nd->getNumKids())) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.Nd->getKid(LoopCounter++));
                break;
              case State::Exit:
                LoopCounterStack.pop();
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpLoop:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                LoopCounterStack.push();
                LoopCounter = size_t(Frame.ReturnValue);
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (LoopCounter-- == 0) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                LoopCounterStack.pop();
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpLoopUnbounded:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (ReadPos.atReadBitEob()) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpIfThen:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpIfThenElse:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.Nd->getKid(1));
                else
                  call(Method::Eval, Frame.Nd->getKid(2));
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpSwitch:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.Nd->getKid(0));
                break;
              case State::Step2: {
                Frame.CallState = State::Exit;
                const auto* Sel = cast<SwitchNode>(Frame.Nd);
                if (const auto* Case = Sel->getCase(Frame.ReturnValue))
                  call(Method::Eval, Case);
                else
                  call(Method::Eval, Sel->getKid(1));
                break;
              }
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpCase:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpDefine:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.Nd->getKid(2));
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpParam:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Eval;
                call(Method::EvalParam, Frame.Nd);
                break;
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpEval:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                TraceEnterFrame();
                auto* Sym = dyn_cast<SymbolNode>(Frame.Nd->getKid(0));
                assert(Sym);
                auto* Defn = dyn_cast<DefineNode>(Sym->getDefineDefinition());
                assert(Defn);
                auto* NumParams = dyn_cast<ParamNode>(Defn->getKid(1));
                assert(NumParams);
                int NumCallArgs = Frame.Nd->getNumKids() - 1;
                if (NumParams->getValue() != IntType(NumCallArgs)) {
                  fprintf(Trace.getFile(), "Definition %s expects %" PRIuMAX
                                           "parameters, found: %" PRIuMAX "\n",
                          Sym->getStringName().c_str(),
                          uintmax_t(NumParams->getValue()),
                          uintmax_t(NumCallArgs));
                  fatal("Unable to evaluate call");
                }
                size_t CallingEvalIndex = CallingEvalStack.size();
                CallingEvalStack.push();
                CallingEval.Caller = cast<EvalNode>(Frame.Nd);
                CallingEval.CallingEvalIndex = CallingEvalIndex;
                Frame.CallState = State::Exit;
                call(Method::Eval, Defn);
                break;
              }
              case State::Exit:
                CallingEvalStack.pop();
                popAndReturn(Frame.ReturnValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpBlock:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
                // NOTE: This assumes that blocks (outside of sections) are only
                // used to define functions.
                TRACE_BLOCK({
                  fprintf(TRACE.indent(), " Function %" PRIuMAX "\n",
                          uintmax_t(LogBlockCount));
                  if (LOG_NUMBERED_BLOCK &&
                      LogBlockCount == LOG_FUNCTION_NUMBER)
                    TRACE.setTraceProgress(true);
                });
#endif
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Eval;
                call(Method::EvalBlock, Frame.Nd->getKid(0));
                break;
              case State::Exit:
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCKS
#if LOG_NUMBERED_BLOCK
                TRACE_BLOCK({
                  if (LogBlockCount == LOG_FUNCTION_NUMBER)
                    TRACE.setTraceProgress(0);
                });
#endif
                ++LogBlockCount;
#endif
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpVoid:  // Method::Eval
            TraceEnterFrame();
            popAndReturn();
            TraceExitFrame();
            break;
        }
        break;
      case Method::EvalBlock:
        switch (Frame.CallState) {
          case State::Enter: {
            TraceEnterFrame();
            const uint32_t OldSize = Reader->readBlockSize(ReadPos);
            TRACE(uint32_t, "block size", OldSize);
            Reader->pushEobAddress(ReadPos, OldSize);
            BlockStartStack.push();
            BlockStart = WritePos;
            Writer->writeFixedBlockSize(WritePos, 0);
            BlockStartStack.push();
            BlockStart = WritePos;
            Frame.CallState =
                MinimizeBlockSize ? State::MinBlock : State::Step2;
            call(DispatchedMethod, Frame.Nd);
            break;
          }
          case State::Step2: {
            // Non-minimized block. Just backpatch in new size.
            WriteCursor WritePosAfterSizeWrite(BlockStart);
            BlockStartStack.pop();
            const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
            TRACE(uint32_t, "New block size", NewSize);
            Writer->writeFixedBlockSize(BlockStart, NewSize);
            Frame.CallState = State::Exit;
            break;
          }
          case State::MinBlock: {
            // Mimimized block. Backpatch new size of block. If needed, move
            // block to fill gap between fixed and variable widths for block
            // size.
            WriteCursor WritePosAfterSizeWrite(BlockStart);
            BlockStartStack.pop();
            const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
            TRACE(uint32_t, "New block size", NewSize);
            Writer->writeVarintBlockSize(BlockStart, NewSize);
            size_t SizeAfterBackPatch = Writer->getStreamAddress(BlockStart);
            size_t SizeAfterSizeWrite =
                Writer->getStreamAddress(WritePosAfterSizeWrite);
            size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
            if (Diff) {
              size_t CurAddress = Writer->getStreamAddress(WritePos);
              Writer->moveBlock(BlockStart, SizeAfterSizeWrite,
                                (CurAddress - Diff) - SizeAfterBackPatch);
              WritePos.swap(BlockStart);
            }
            Frame.CallState = State::Exit;
            break;
          }
          case State::Exit:
            BlockStartStack.pop();
            ReadPos.popEobAddress();
            popAndReturn();
            TraceExitFrame();
            break;
          default:
            fail("Bad internal state for method EvalBlock");
            break;
        }
        break;
      case Method::Finished:
        assert(FrameStack.empty());
        switch (Frame.CallState) {
          case State::Succeeded:
          case State::Failed:
            break;
          default:
            TRACE(string, "State", StateName[int(Frame.CallState)]);
            TRACE_MESSAGE("Malformed finish state found, Correcting!");
            Frame.CallState = State::Failed;
        }
#if LOG_RUNMETHODS
        TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
        TRACE_EXIT_OVERRIDE("runMethods");
#endif
        return;
      case Method::EvalParam:
        switch (Frame.CallState) {
          case State::Enter: {
            TraceEnterFrame();
            if (CallingEvalStack.empty()) {
              fail(
                  "Not inside a call frame, can't evaluate parameter "
                  "accessor!");
              break;
            }
            Frame.CallState = State::Failed;  // reset if match found.
            assert(isa<ParamNode>(Frame.Nd));
            auto* Param = cast<ParamNode>(Frame.Nd);
            TRACE_SEXP("Param", Param);
            IntType ParamIndex = Param->getValue() + 1;
            SymbolNode* DefiningSym = Param->getDefiningSymbol();
            TRACE_SEXP("DefiningSym", DefiningSym);
            const EvalFrame* CandidateEval = &CallingEval;
            while (CandidateEval->isDefined()) {
              TRACE_SEXP("Caller", CandidateEval->Caller);
              if (DefiningSym != CandidateEval->Caller->getCallName()) {
                CandidateEval =
                    &CallingEvalStack.at(CandidateEval->CallingEvalIndex);
                continue;
              }
              if (ParamIndex >= IntType(CandidateEval->Caller->getNumKids()))
                // Failing context!
                break;
              const Node* Context = CandidateEval->Caller->getKid(ParamIndex);
              CallingEvalStack.push();
              CallingEval =
                  CallingEvalStack.at(CandidateEval->CallingEvalIndex);
              Frame.CallState = State::Exit;
              call(DispatchedMethod, Context);
              break;
            }
            if (Frame.CallState == State::Failed)
              fail("Parameter reference doesn't match callling context!");
            break;
          }
          case State::Exit:
            CallingEvalStack.pop();
            popAndReturn(Frame.ReturnValue);
            TraceExitFrame();
            break;
          default:
            fail("Bad internal state for method GetParam");
            break;
        }
        break;
      case Method::Read:
        switch (Frame.Nd->getType()) {
          case OpBlock:
          case OpBlockEndNoArgs:
          case OpCase:
          case OpConvert:
          case OpDefine:
          case OpFile:
          case OpFilter:
          case OpIfThen:
          case OpIfThenElse:
          case OpLoop:
          case OpLoopUnbounded:
          case OpRename:
          case OpSection:
          case OpSequence:
          case OpSwitch:
          case OpSymbol:
          case OpUndefine:
          case OpUnknownSection:
          case OpVersion:
          case NO_SUCH_NODETYPE:  // Method::Read
            fail("Construct not readable!");
            break;
          case OpAnd:
          case OpError:
          case OpEval:
          case OpNot:
          case OpOr:
          case OpRead:
          case OpStream:
          case OpWrite:  // Method::Read
            fail("Read not implemented for construct!");
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(dyn_cast<IntegerNode>(Frame.Nd)->getValue());
            TraceExitFrame();
            break;
          case OpParam:  // Method::Read
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Read;
                call(Method::EvalParam, Frame.Nd);
                break;
              case State::Exit:
                popAndReturnReadValue(LastReadValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Write");
                break;
            }
            break;
          case OpPeek: {  // Method::Read
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                PeekPosStack.push();
                PeekPos = ReadPos;
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd->getKid(0));
                break;
              case State::Exit:
                ReadPos = PeekPos;
                PeekPosStack.pop();
                popAndReturnReadValue(LastReadValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Read");
                break;
            }
            break;
          }
          case OpLastRead:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(LastReadValue);
            TraceExitFrame();
            break;
          case OpUint8NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint8(ReadPos));
            TraceExitFrame();
            break;
          case OpUint8OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint8Bits(
                ReadPos, cast<Uint8OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpUint32NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint32(ReadPos));
            TraceExitFrame();
            break;
          case OpUint32OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint32Bits(
                ReadPos, cast<Uint32OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpUint64NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint64(ReadPos));
            TraceExitFrame();
            break;
          case OpUint64OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readUint64Bits(
                ReadPos, cast<Uint64OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpVarint32NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVarint32(ReadPos));
            TraceExitFrame();
            break;
          case OpVarint32OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVarint32Bits(
                ReadPos, cast<Varint32OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpVarint64NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVarint64(ReadPos));
            TraceExitFrame();
            break;
          case OpVarint64OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVarint64Bits(
                ReadPos, cast<Varint64OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpVaruint32NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVaruint32(ReadPos));
            TraceExitFrame();
            break;
          case OpVaruint32OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVaruint32Bits(
                ReadPos, cast<Varuint32OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpVaruint64NoArgs:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVaruint64(ReadPos));
            TraceExitFrame();
            break;
          case OpVaruint64OneArg:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(Reader->readVaruint64Bits(
                ReadPos, cast<Varuint64OneArgNode>(Frame.Nd)->getValue()));
            TraceExitFrame();
            break;
          case OpOpcode:  // Method::Read
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                OpcodeLocalsStack.push();
                OpcodeLocals.reset();
                Frame.CallState = State::Exit;
                call(Method::ReadOpcode, Frame.Nd);
                break;
              case State::Exit:
                LastReadValue = OpcodeLocals.CaseMask;
                OpcodeLocalsStack.pop();
                popAndReturn(LastReadValue);
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Eval");
                break;
            }
            break;
          case OpMap: {  // Method::Read
            // TODO: Flatten this.
            TraceEnterFrame();
            const auto* Map = cast<MapNode>(Frame.Nd);
            const CaseNode* Case = Map->getCase(read(Map->getKid(0)));
            popAndReturnReadValue(read(Case->getKid(1)));
            TraceExitFrame();
            break;
          }
          case OpVoid:  // Method::Read
            TraceEnterFrame();
            popAndReturnReadValue(0);
            TraceExitFrame();
            break;
        }
        break;
      case Method::ReadOpcode:
        // Note: Assumes that caller pushes OpcodeLocals;
        switch (Frame.Nd->getType()) {
          default:
            fail("Illegal opcode construct!");
            break;
          case OpOpcode:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::ReadOpcode, Frame.Nd->getKid(0));
                break;
              case State::Step2: {
                const auto* Sel = cast<OpcodeNode>(Frame.Nd);
                if (const CaseNode* Case =
                        Sel->getCase(OpcodeLocals.CaseMask)) {
                  Frame.CallState = State::Step3;
                  OpcodeLocalsStack.push();
                  call(Method::ReadOpcode, Case);
                  break;
                }
                Frame.CallState = State::Exit;
                break;
              }
              case State::Step3: {
                OpcodeLocalsFrame CaseResults = OpcodeLocals;
                OpcodeLocalsStack.pop();
                OpcodeLocals.CaseMask =
                    (OpcodeLocals.CaseMask << CaseResults.SelShift) |
                    CaseResults.CaseMask;
                OpcodeLocals.SelShift += CaseResults.SelShift;
                Frame.CallState = State::Exit;
                break;
              }
              case State::Exit:
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
          case OpUint8NoArgs:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = 8;
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
            break;
          case OpUint8OneArg:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift =
                    cast<Uint8OneArgNode>(Frame.Nd)->getValue();
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
          case OpUint32NoArgs:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = 32;
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
            break;
          case OpUint32OneArg:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift =
                    cast<Uint32OneArgNode>(Frame.Nd)->getValue();
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
            break;
          case OpUint64NoArgs:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = 64;
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
            break;
          case OpUint64OneArg:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Read, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift =
                    cast<Uint64OneArgNode>(Frame.Nd)->getValue();
                popAndReturn();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method ReadOpcode");
                break;
            }
            break;
        }
        break;
      case Method::Write:
        switch (Frame.Nd->getType()) {
          case OpBlock:
          case OpBlockEndNoArgs:
          case OpCase:
          case OpConvert:
          case OpDefine:
          case OpFile:
          case OpFilter:
          case OpIfThen:
          case OpIfThenElse:
          case OpLoop:
          case OpLoopUnbounded:
          case OpRename:
          case OpSection:
          case OpSequence:
          case OpSwitch:
          case OpSymbol:
          case OpUndefine:
          case OpUnknownSection:
          case OpVersion:
          case NO_SUCH_NODETYPE:  // Method::Write
            fail("Construct not readable!");
            break;
          case OpAnd:
          case OpError:
          case OpEval:
          case OpLastRead:
          case OpNot:
          case OpOr:
          case OpRead:
          case OpStream:
          case OpWrite:  // Method::Write
            fail("Write not implemented for construct!");
            break;
          case OpParam:  // Method::Write
            switch (Frame.CallState) {
              case State::Enter:
                TraceEnterFrame();
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Write;
                callWrite(Method::EvalParam, Frame.Nd, WriteValue);
                break;
              case State::Exit:
                popAndReturnWriteValue();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Write");
                break;
            }
            break;
          case OpUint8NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint8(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpUint8OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint8Bits(WriteValue, WritePos,
                                   cast<Uint8OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpUint32NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint32(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpUint32OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint32Bits(
                WriteValue, WritePos,
                cast<Uint32OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpUint64NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint64(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpUint64OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeUint64Bits(
                WriteValue, WritePos,
                cast<Uint64OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVarint32NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeVarint32(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVarint32OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeVarint32Bits(
                WriteValue, WritePos,
                cast<Varint32OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVarint64NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeVarint64(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVarint64OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeVarint64Bits(
                WriteValue, WritePos,
                cast<Varint64OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVaruint32NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeVaruint32(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVaruint32OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeVaruint32Bits(
                WriteValue, WritePos,
                cast<Varuint32OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVaruint64NoArgs:  // Method::Write
            TraceEnterFrame();
            Writer->writeVaruint64(WriteValue, WritePos);
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpVaruint64OneArg:  // Method::Write
            TraceEnterFrame();
            Writer->writeVaruint64Bits(
                WriteValue, WritePos,
                cast<Varuint64OneArgNode>(Frame.Nd)->getValue());
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const:
          case OpMap:
          case OpPeek:
          case OpVoid:  // Method::Write
            TraceEnterFrame();
            popAndReturnWriteValue();
            TraceExitFrame();
            break;
          case OpOpcode: {  // Method::Write
            switch (Frame.CallState) {
              case State::Enter: {
                TraceEnterFrame();
                const auto* Sel = cast<OpcodeNode>(Frame.Nd);
                OpcodeLocalsStack.push();
                OpcodeLocals.Case = Sel->getWriteCase(
                    WriteValue, OpcodeLocals.SelShift, OpcodeLocals.CaseMask);
                WriteValueStack.push();
                WriteValue >>= OpcodeLocals.SelShift;
                Frame.CallState = State::Exit;
                callWrite(Method::Write, Sel->getKid(0), WriteValue);
                break;
              }
              case State::Step2:
                OpcodeLocalsStack.pop();
                Frame.CallState = State::Exit;
                if (OpcodeLocals.Case) {
                  WriteValueStack.push();
                  WriteValue &= OpcodeLocals.CaseMask;
                  callWrite(Method::Write, OpcodeLocals.Case->getKid(1),
                            WriteValue);
                }
                break;
              case State::Exit:
                popAndReturnWriteValue();
                TraceExitFrame();
                break;
              default:
                fail("Bad internal state for method Write");
                break;
            }
            break;
          }
        }
        break;
    }
  }
#if LOG_RUNMETHODS
  TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
  TRACE_EXIT_OVERRIDE("runMethods");
#endif
}

void Interpreter::decompress() {
  TRACE_METHOD("decompress");
  LastReadValue = 0;
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  TRACE(hex_uint32_t, "magic number", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to decompress, did not find WASM binary magic number");
  Writer->writeUint32(MagicNumber, WritePos);
  Version = Reader->readUint32(ReadPos);
  TRACE(hex_uint32_t, "version", Version);
  if (Version != WasmBinaryVersion)
    fatal("Unable to decompress, WASM version number not known");
  Writer->writeUint32(Version, WritePos);

  while (!ReadPos.atByteEob())
    decompressSection();
  WritePos.freezeEof();
}

void Interpreter::decompressBlock(const Node* Code) {
  TRACE_METHOD("decompressBlock");
  const uint32_t OldSize = Reader->readBlockSize(ReadPos);
  TRACE(uint32_t, "block size", OldSize);
  Reader->pushEobAddress(ReadPos, OldSize);
  WriteCursor BlockStart(WritePos);
  Writer->writeFixedBlockSize(WritePos, 0);
  size_t SizeAfterSizeWrite = Writer->getStreamAddress(WritePos);
  evalOrCopy(Code);
  const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
  TRACE(uint32_t, "New block size", NewSize);
  if (!MinimizeBlockSize) {
    Writer->writeFixedBlockSize(BlockStart, NewSize);
  } else {
    Writer->writeVarintBlockSize(BlockStart, NewSize);
    size_t SizeAfterBackPatch = Writer->getStreamAddress(BlockStart);
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t CurAddress = Writer->getStreamAddress(WritePos);
      Writer->moveBlock(BlockStart, SizeAfterSizeWrite,
                        (CurAddress - Diff) - SizeAfterBackPatch);
      WritePos.swap(BlockStart);
    }
  }
  ReadPos.popEobAddress();
}

void Interpreter::evalOrCopy(const Node* Nd) {
  if (Nd) {
    eval(Nd);
    return;
  }
  // If not defined, must be at end of section, and hence byte aligned.
  while (!ReadPos.atByteEob())
    Writer->writeUint8(Reader->readUint8(ReadPos), WritePos);
}

void Interpreter::decompressSection() {
  // TODO(kschimpf) Handle 'filter' sections specially (i.e. install).  This
  // includes calling "clearCaches" on all filter s-expressions to remove an
  // (optimizing) caches installed.
  TRACE_METHOD("decompressSection");
  LastReadValue = 0;
  assert(isa<ByteReadStream>(Reader.get()));
#if LOG_SECTIONS
  size_t SectionAddress = ReadPos.getCurByteAddress();
#endif
  readSectionName();
#if LOG_SECTIONS
  TRACE_BLOCK({
    fprintf(TRACE.indent(), "@%" PRIxMAX " section '%s'\n",
            uintmax_t(SectionAddress), CurSectionName.c_str());
  });
#endif
  TRACE(string, "name", CurSectionName);
  SymbolNode* Sym = Symtab->getSymbol(CurSectionName);
  decompressBlock(Sym ? Sym->getDefineDefinition() : nullptr);
  Reader->alignToByte(ReadPos);
  Writer->alignToByte(WritePos);
}

void Interpreter::readSectionName() {
  TRACE_METHOD("readSectionName");
  CurSectionName.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  Writer->writeVaruint32(NameSize, WritePos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Writer->writeUint8(Byte, WritePos);
    CurSectionName.push_back(char(Byte));
  }
}

}  // end of namespace interp.

}  // end of namespace wasm.
