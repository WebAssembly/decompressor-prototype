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

#include "interp/Interpreter.h"

#include "interp/ByteReadStream.h"
#include "interp/ByteWriteStream.h"
#include "sexp/TextWriter.h"
#include "utils/backwards_iterator.h"

#include <iostream>

// By default, methods resume() and readBackFilled() are not traced,
// since they are the glue between a push and pull models. Rather, they
// conceptually mimic the natural call structure. If you want to trace
// resume() and readBackFilled() as well, change this flag to 1.
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

// The following shows stack contents on each iteration of resume();
#define LOG_CALLSTACKS 0

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

namespace {

static constexpr size_t DefaultStackSize = 256;

}  // end of anonymous namespace

Interpreter::Interpreter(std::shared_ptr<Queue> Input,
                         std::shared_ptr<Queue> Output,
                         std::shared_ptr<SymbolTable> Symtab)
    : Reader(Input, Symtab, Trace),
      Symtab(Symtab),
      WritePos(StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      MinimizeBlockSize(false),
      Trace(ReadPos, WritePos, "InterpSexp"),
      WriteValueStack(WriteValue),
      BlockStartStack(BlockStart) {
  DefaultFormat = Symtab->getVaruint64Definition();
  WriteValueStack.reserve(DefaultStackSize);
}

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

void Interpreter::describeWriteValueStack(FILE* File) {
  fprintf(File, "*** WriteValue Stack ***\n");
  for (auto& Value : WriteValueStack.iterRange(1))
    fprintf(File, "%" PRIuMAX "\n", uintmax_t(Value));
  fprintf(File, "************************\n");
}

void Interpreter::describeBlockStartStack(FILE* File) {
  fprintf(File, "*** Block Start Stack ***\n");
  for (const auto& Pos : BlockStartStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "*************************\n");
}

void Interpreter::describeAllNonemptyStacks(FILE* File) {
  Reader::describeAllNonemptyStacks(File);
  if (!WriteValueStack.empty())
    describeWriteValueStack(File);
  if (!BlockStartStack.empty())
    describeBlockStartStack(File);
}

void Interpreter::clearStacksExceptFrame() {
  WriteValue = 0;
  WriteValueStack.clear();
  BlockStart = WriteCursor();
  BlockStartStack.clear();
}

void Interpreter::readBackFilled() {
#if LOG_RUNMETHODS
  TRACE_METHOD("readBackFilled");
#endif
  ReadCursor FillPos(ReadPos);
  while (!isFinished()) {
    if (!FillPos.atEof())
      FillPos.advance(Page::Size);
    resume();
  }
}

void Interpreter::resume() {
#if LOG_RUNMETHODS
  TRACE_ENTER("resume");
  TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
#endif
  size_t FillPos = ReadPos.fillSize();
  // Headroom is used to guarantee that several (integer) reads
  // can be done in a single iteration of the loop.
  constexpr size_t kResumeHeadroom = 100;
  if (!ReadPos.isEofFrozen()) {
    if (FillPos < kResumeHeadroom)
      return;
    FillPos -= kResumeHeadroom;
  }
  while (ReadPos.getCurByteAddress() <= FillPos) {
    if (errorsFound())
      break;
#if LOG_CALLSTACKS
    TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
#endif
    switch (Frame.CallMethod) {
      case Method::NO_SUCH_METHOD:
        failNotImplemented();
        break;
      case Method::CopyBlock:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            Frame.CallState = State::Loop;
            break;
          case State::Loop:
            if (ReadPos.atByteEob()) {
              Frame.CallState = State::Exit;
              break;
            }
            Writer->writeUint8(InputReader->readUint8(ReadPos), WritePos);
            break;
          case State::Exit:
            popAndReturn();
            traceExitFrame();
            break;
          default:
            failBadState();
            break;
        }
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
          case OpBitwiseAnd:
          case OpBitwiseNegate:
          case OpBitwiseOr:
          case OpBitwiseXor:
          case OpConvert:
          case OpParams:
          case OpFilter:  // Method::Eval
            failNotImplemented();
            break;
          case NO_SUCH_NODETYPE:
          case OpFile:
          case OpLocals:
          case OpRename:
          case OpSymbol:
          case OpSection:
          case OpUndefine:
          case OpUnknownSection:
          case OpCasmVersion:
          case OpWasmVersion:  // Method::Eval
            failNotImplemented();
            break;
          case OpError:  // Method::Eval
            traceEnterFrame();
            fail("Algorithm error!");
            break;
          case OpCallback:  // Method::Eval
            // TODO(karlschimpf): All virtual calls to class so that derived
            // classes can override.
            traceEnterFrame();
            popAndReturn(Frame.ReturnValue);
            traceExitFrame();
            break;
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const: {  // Method::Eval
            traceEnterFrame();
            IntType Value = dyn_cast<IntegerNode>(Frame.Nd)->getValue();
            if (hasReadMode())
              LastReadValue = Value;
            popAndReturn(Value);
            traceExitFrame();
            break;
          }
          case OpLastRead:
            traceEnterFrame();
            popAndReturn(LastReadValue);
            traceExitFrame();
            break;
          case OpLocal: {
            traceEnterFrame();
            const auto* Local = dyn_cast<LocalNode>(Frame.Nd);
            size_t Index = Local->getValue();
            if (LocalsBase + Index >= LocalValues.size()) {
              fail("Local variable index out of range!");
              break;
            }
            popAndReturn(LocalValues[LocalsBase + Index]);
            traceExitFrame();
            break;
          }
          case OpPeek:
            traceEnterFrame();
            switch (Frame.CallState) {
              case State::Enter:
                PeekPosStack.push(ReadPos);
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Exit:
                ReadPos = PeekPos;
                PeekPosStack.pop();
                popAndReturn(Frame.ReturnValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            traceExitFrame();
            break;
          case OpRead:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpUint32:
          case OpUint64:
          case OpUint8:
          case OpVarint32:
          case OpVarint64:
          case OpVaruint32:
          case OpVaruint64: {
            traceEnterFrame();
            IntType Value = InputReader->readValue(ReadPos, Frame.Nd);
            if (hasReadMode())
              LastReadValue = Value;
            if (hasWriteMode()) {
              if (!Writer->writeValue(LastReadValue, WritePos, Frame.Nd)) {
                failCantWrite();
                break;
              }
            }
            popAndReturn(Value);
            traceExitFrame();
            break;
          }
          case OpMap:
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                if (hasReadMode())
                  call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (hasReadMode()) {
                  LastReadValue = Frame.ReturnValue;
                  call(Method::Eval, MethodModifier::ReadOnly,
                       cast<MapNode>(Frame.Nd)->getCase(LastReadValue));
                }
                break;
              case State::Exit:
                if (hasReadMode())
                  LastReadValue = Frame.ReturnValue;
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpOpcode:  // Method::Eval
            fail("Multibyte opcodes broken!");
            break;
          case OpSet:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                const auto* Local = dyn_cast<LocalNode>(Frame.Nd);
                size_t Index = Local->getValue();
                if (LocalsBase + Index >= LocalValues.size()) {
                  fail("Local variable index out of range, can't set!");
                  break;
                }
                LocalValues[LocalsBase + Index] = Frame.ReturnValue;
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              }
              default:
                failBadState();
                break;
            }
            break;
          case OpWrite:  // Method::Eval
            // TODO(karlschimpf) Generalize this to accept arbitrary
            // expressions.
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::WriteOnly,
                     Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpStream: {  // Method::Eval
            traceEnterFrame();
            const auto* Stream = cast<StreamNode>(Frame.Nd);
            IntType Result = 0;
            switch (Stream->getStreamKind()) {
              case StreamKind::Input:
                switch (Stream->getStreamType()) {
                  case StreamType::Byte:
                    Result = isa<ByteReadStream>(InputReader.get());
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
                    Result = isa<ByteReadStream>(Writer.get());
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
            popAndReturn(Result);
            break;
          }
          case OpNot:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                traceExitFrame();
                break;
              }
              default:
                failBadState();
                break;
            }
            break;
          case OpAnd:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, MethodModifier::ReadOnly,
                       Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                traceExitFrame();
                break;
              }
              default:
                failBadState();
                break;
            }
            break;
          case OpOr:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue == 0)
                  call(Method::Eval, MethodModifier::ReadOnly,
                       Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                traceExitFrame();
                break;
              }
              default:
                failBadState();
                break;
            }
            break;
          case OpSequence:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                LoopCounterStack.push(0);
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (LoopCounter >= size_t(Frame.Nd->getNumKids())) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.CallModifier,
                     Frame.Nd->getKid(LoopCounter++));
                break;
              case State::Exit:
                LoopCounterStack.pop();
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpLoop:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                LoopCounterStack.push(Frame.ReturnValue);
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (LoopCounter-- == 0) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                LoopCounterStack.pop();
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpLoopUnbounded:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (ReadPos.atReadBitEob()) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpIfThen:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpIfThenElse:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue)
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                else
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(2));
                break;
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpSwitch:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2: {
                Frame.CallState = State::Exit;
                const auto* Sel = cast<SwitchNode>(Frame.Nd);
                if (const auto* Case = Sel->getCase(Frame.ReturnValue))
                  call(Method::Eval, Frame.CallModifier, Case);
                else
                  call(Method::Eval, Frame.CallModifier, Sel->getKid(1));
                break;
              }
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpCase:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpDefine:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                if (size_t NumLocals =
                        cast<DefineNode>(Frame.Nd)->getNumLocals()) {
                  LocalsBaseStack.push(LocalValues.size());
                  for (size_t i = 0; i < NumLocals; ++i)
                    LocalValues.push_back(0);
                }
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(2));
                break;
              case State::Exit:
                if (cast<DefineNode>(Frame.Nd)->getNumLocals()) {
                  while (LocalValues.size() > LocalsBase)
                    LocalValues.pop_back();
                  LocalsBaseStack.pop();
                }
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpParam:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Eval;
                call(Method::EvalParam, Frame.CallModifier, Frame.Nd);
                break;
              case State::Exit:
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpEval:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                traceEnterFrame();
                auto* Sym = dyn_cast<SymbolNode>(Frame.Nd->getKid(0));
                assert(Sym);
                auto* Defn = dyn_cast<DefineNode>(Sym->getDefineDefinition());
                assert(Defn);
                auto* NumParams = dyn_cast<ParamsNode>(Defn->getKid(1));
                assert(NumParams);
                int NumCallArgs = Frame.Nd->getNumKids() - 1;
                if (NumParams->getValue() != IntType(NumCallArgs)) {
                  fprintf(Trace.getFile(), "Definition %s expects %" PRIuMAX
                                           "parameters, found: %" PRIuMAX "\n",
                          Sym->getStringName().c_str(),
                          uintmax_t(NumParams->getValue()),
                          uintmax_t(NumCallArgs));
                  fail("Unable to evaluate call");
                }
                size_t CallingEvalIndex = CallingEvalStack.size();
                CallingEvalStack.push();
                CallingEval.Caller = cast<EvalNode>(Frame.Nd);
                CallingEval.CallingEvalIndex = CallingEvalIndex;
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Defn);
                break;
              }
              case State::Exit:
                CallingEvalStack.pop();
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpBlock:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
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
                call(Method::EvalBlock, Frame.CallModifier,
                     Frame.Nd->getKid(0));
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
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpVoid:  // Method::Eval
            traceEnterFrame();
            popAndReturn();
            traceExitFrame();
            break;
        }
        break;
      case Method::EvalBlock:
        switch (Frame.CallState) {
          case State::Enter: {
            traceEnterFrame();
            const uint32_t OldSize = InputReader->readBlockSize(ReadPos);
            TRACE(uint32_t, "block size", OldSize);
            InputReader->pushEobAddress(ReadPos, OldSize);
            BlockStartStack.push(WritePos);
            Writer->writeFixedBlockSize(WritePos, 0);
            BlockStartStack.push(WritePos);
            Frame.CallState =
                MinimizeBlockSize ? State::MinBlock : State::Step2;
            call(DispatchedMethod, Frame.CallModifier, Frame.Nd);
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
            traceExitFrame();
            break;
          default:
            failBadState();
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
            TRACE(string, "State", getName(Frame.CallState));
            TRACE_MESSAGE("Malformed finish state found, Correcting!");
            Frame.CallState = State::Failed;
        }
#if LOG_RUNMETHODS
        TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
        TRACE_EXIT_OVERRIDE("resume");
#endif
        return;
      case Method::EvalParam:
        switch (Frame.CallState) {
          case State::Enter: {
            traceEnterFrame();
            if (CallingEvalStack.empty()) {
              fail(
                  "Not inside a call frame, can't evaluate parameter "
                  "accessor!");
              break;
            }
            assert(isa<ParamNode>(Frame.Nd));
            auto* Param = cast<ParamNode>(Frame.Nd);
            IntType ParamIndex = Param->getValue() + 1;
            if (ParamIndex >= IntType(CallingEval.Caller->getNumKids())) {
              fail("Parameter reference doesn't match callling context!");
              break;
            }
            const Node* Context = CallingEval.Caller->getKid(ParamIndex);
            CallingEvalStack.push(
                CallingEvalStack.at(CallingEval.CallingEvalIndex));
            Frame.CallState = State::Exit;
            call(DispatchedMethod, Frame.CallModifier, Context);
            break;
          }
          case State::Exit:
            CallingEvalStack.pop();
            popAndReturn(Frame.ReturnValue);
            traceExitFrame();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::GetFile:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            MagicNumber = InputReader->readUint32(ReadPos);
            TRACE(hex_uint32_t, "magic number", MagicNumber);
            if (MagicNumber != WasmBinaryMagic) {
              fail(
                  "Unable to compress. Did not find WASM binary magic number!");
              break;
            }
            Writer->writeUint32(MagicNumber, WritePos);
            Version = InputReader->readUint32(ReadPos);
            TRACE(hex_uint32_t, "version", Version);
            if (Version != WasmBinaryVersion) {
              fail("Unable to compress. WASM version not known");
              break;
            }
            Writer->writeUint32(Version, WritePos);
            Frame.CallState = State::Loop;
            break;
          case State::Loop:
            if (ReadPos.atByteEob()) {
              Frame.CallState = State::Exit;
              break;
            }
            call(Method::GetSection, Frame.CallModifier, nullptr);
            break;
          case State::Exit:
            WritePos.freezeEof();
            popAndReturn();
            traceExitFrame();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::GetSecName:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            CurSectionName.clear();
            LoopCounterStack.push(InputReader->readVaruint32(ReadPos));
            Writer->writeVaruint32(LoopCounter, WritePos);
            Frame.CallState = State::Loop;
            break;
          case State::Loop: {
            if (LoopCounter == 0) {
              Frame.CallState = State::Exit;
              break;
            }
            --LoopCounter;
            uint8_t Byte = InputReader->readUint8(ReadPos);
            Writer->writeUint8(Byte, WritePos);
            CurSectionName.push_back(char(Byte));
            break;
          }
          case State::Exit:
            popAndReturn();
            traceExitFrame();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::GetSection:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            assert(isa<ByteReadStream>(InputReader.get()));
#if LOG_SECTIONS
            TRACE(hex_size_t, "SectionAddress", ReadPos.getCurByteAddress());
#endif
            Frame.CallState = State::Step2;
            call(Method::GetSecName, Frame.CallModifier, nullptr);
            break;
          case State::Step2: {
            TRACE(string, "Section", CurSectionName);
            // TODO(kschimpf) Handle 'filter' sections specially (i.e. install).
            SymbolNode* Sym = Symtab->getSymbol(CurSectionName);
            const Node* Algorithm = nullptr;
            if (Sym)
              Algorithm = Sym->getDefineDefinition();
            DispatchedMethod = Algorithm ? Method::Eval : Method::CopyBlock;
            Frame.CallState = State::Exit;
            call(Method::EvalBlock, Frame.CallModifier, Algorithm);
            break;
          }
          case State::Exit:
            InputReader->alignToByte(ReadPos);
            Writer->alignToByte(WritePos);
            popAndReturn();
            traceExitFrame();
            break;
          default:
            failBadState();
            break;
        }
        break;
      case Method::ReadOpcode:
        // Note: Assumes that caller pushes OpcodeLocals;
        switch (Frame.Nd->getType()) {
          default:
            failNotImplemented();
            break;
          case OpOpcode:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Step2;
                call(Method::ReadOpcode, Frame.CallModifier,
                     Frame.Nd->getKid(0));
                break;
              case State::Step2: {
                const auto* Sel = cast<OpcodeNode>(Frame.Nd);
                if (const CaseNode* Case =
                        Sel->getCase(OpcodeLocals.CaseMask)) {
                  Frame.CallState = State::Step3;
                  OpcodeLocalsStack.push();
                  call(Method::ReadOpcode, Frame.CallModifier, Case);
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
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
          case OpUint8:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = cast<Uint8Node>(Frame.Nd)->getValue();
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
          case OpUint32:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = cast<Uint32Node>(Frame.Nd)->getValue();
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
          case OpUint64:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                traceEnterFrame();
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = cast<Uint64Node>(Frame.Nd)->getValue();
                popAndReturn();
                traceExitFrame();
                break;
              default:
                failBadState();
                break;
            }
            break;
        }
        break;
      case Method::Started:
        // If reached, we finished processing the input.
        assert(FrameStack.empty());
        Frame.CallMethod = Method::Finished;
        if (ReadPos.atEof() && ReadPos.isQueueGood())
          Frame.CallState = State::Succeeded;
        else
          fail("Malformed input in compressed file");
        break;
    }
  }
#if LOG_RUNMETHODS
  TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
  TRACE_EXIT_OVERRIDE("resume");
#endif
}

void Interpreter::decompress() {
  start();
  readBackFilled();
}

}  // end of namespace interp.

}  // end of namespace wasm.
