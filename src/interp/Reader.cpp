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

// implements a reader for wasm/casm files.

#include "interp/Reader.h"

#include "interp/ByteReadStream.h"
#include "sexp/TextWriter.h"

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

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

namespace {

static constexpr uint32_t MaxExpectedSectionNameSize = 32;

static constexpr size_t DefaultStackSize = 256;
static constexpr size_t DefaultExpectedLocals = 3;

const char* SectionCodeName[] = {
#define X(code, value) #code
    SECTION_CODES_TABLE
#undef X
};

const char* MethodName[] = {
#define X(tag) #tag,
    INTERPRETER_METHODS_TABLE
#undef X
    "NO_SUCH_METHOD"};

const char* StateName[] = {
#define X(tag) #tag,
    INTERPRETER_STATES_TABLE
#undef X
    "NO_SUCH_STATE"};

const char* MethodModifierName[] = {
#define X(tag, flags) #tag,
    INTERPRETER_METHOD_MODIFIERS_TABLE
#undef X
    "NO_SUCH_METHOD_MODIFIER"};

}  // end of anonymous namespace

const char* Reader::getName(Method M) {
  size_t Index = size_t(M);
  if (Index >= size(MethodName))
    Index = size_t(Method::NO_SUCH_METHOD);
  return MethodName[Index];
}

const char* Reader::getName(MethodModifier Modifier) {
  size_t Index = size_t(Modifier);
  if (Index >= size(MethodModifierName))
    Index = size_t(MethodModifier::NO_SUCH_METHOD_MODIFIER);
  return MethodModifierName[Index];
}

const char* Reader::getName(State S) {
  size_t Index = size_t(S);
  if (Index >= size(StateName))
    Index = size_t(State::NO_SUCH_STATE);
  return StateName[Index];
}

const char* Reader::getName(SectionCode Code) {
  size_t Index = size_t(Code);
  if (Index >= size(SectionCodeName))
    Index = size_t(SectionCode::NO_SUCH_SECTION_CODE);
  return SectionCodeName[Index];
}

Reader::Reader(std::shared_ptr<decode::Queue> StrmInput,
               Writer& StrmOutput,
               std::shared_ptr<filt::SymbolTable> Symtab,
               TraceClassSexp& Trace)
    : ReadPos(StreamType::Byte, StrmInput),
      Input(std::make_shared<ByteReadStream>()),
      Output(StrmOutput),
      Symtab(Symtab),
      LastReadValue(0),
      DispatchedMethod(Method::NO_SUCH_METHOD),
      Trace(Trace),
      FrameStack(Frame),
      CallingEvalStack(CallingEval),
      PeekPosStack(PeekPos),
      LoopCounter(0),
      LoopCounterStack(LoopCounter),
      LocalsBase(0),
      LocalsBaseStack(LocalsBase),
      OpcodeLocalsStack(OpcodeLocals) {
  CurSectionName.reserve(MaxExpectedSectionNameSize);
  FrameStack.reserve(DefaultStackSize);
  CallingEvalStack.reserve(DefaultStackSize);
  LocalsBaseStack.reserve(DefaultStackSize);
  LocalValues.reserve(DefaultStackSize * DefaultExpectedLocals);
  OpcodeLocalsStack.reserve(DefaultStackSize);
}

void Reader::traceEnterFrameInternal() {
  // Note: Enclosed in TRACE_BLOCK so that g++ will not complain when
  // compiled in release mode.
  TRACE_BLOCK({
    TRACE_ENTER(getName(Frame.CallMethod));
    TRACE_SEXP("Nd", Frame.Nd);
    if (Frame.CallModifier != MethodModifier::ReadAndWrite)
      TRACE_MESSAGE(std::string("(") + getName(Frame.CallModifier) + ")");
  });
}

void Reader::CallFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "%s.%s (%s) = %" PRIuMAX ": ", getName(CallMethod),
          getName(CallState), getName(CallModifier), uintmax_t(ReturnValue));
  if (Nd)
    Writer->writeAbbrev(File, Nd);
  else
    fprintf(File, "nullptr\n");
}

void Reader::EvalFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "cc = %" PRIuMAX ": ", uintmax_t(CallingEvalIndex));
  Writer->writeAbbrev(File, Caller);
}

void Reader::OpcodeLocalsFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "OpcodeFrame <%" PRIuMAX ",%" PRIuMAX "> ", uintmax_t(SelShift),
          uintmax_t(CaseMask));
  if (Writer && Case != nullptr)
    Writer->writeAbbrev(File, Case);
  else
    fprintf(File, "%p\n", (void*)Case);
}

void Reader::describeFrameStack(FILE* File) {
  fprintf(File, "*** Frame Stack ***\n");
  for (auto& Frame : FrameStack)
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "*******************\n");
}

void Reader::describeCallingEvalStack(FILE* File) {
  fprintf(File, "*** Eval Call Stack ****\n");
  for (const auto& Frame : CallingEvalStack.iterRange(1))
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "************************\n");
}

void Reader::describePeekPosStack(FILE* File) {
  fprintf(File, "*** Peek Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : PeekPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "**********************\n");
}

void Reader::describeLoopCounterStack(FILE* File) {
  fprintf(File, "*** Loop Counter Stack ***\n");
  for (const auto& Count : LoopCounterStack.iterRange(1))
    fprintf(File, "%" PRIxMAX "\n", uintmax_t(Count));
  fprintf(File, "**************************\n");
}

void Reader::describeLocalsStack(FILE* File) {
  fprintf(File, "*** Locals Base Stack ***\n");
  size_t BaseIndex = 0;
  for (const auto& Index : LocalsBaseStack.iterRange(1)) {
    fprintf(File, "%" PRIuMAX ":\n", uintmax_t(Index));
    for (size_t i = BaseIndex; i < Index; ++i) {
      fprintf(File, "  %" PRIuMAX "\n", LocalValues[i]);
    }
  }
  fprintf(File, "*************************\n");
}

void Reader::describeOpcodeLocalStack(FILE* File) {
  fprintf(File, "*** Opcode Stack ***\n");
  for (auto& Frame : OpcodeLocalsStack.iterRange(1))
    Frame.describe(File, getTrace().getTextWriter());
  fprintf(File, "********************\n");
}

void Reader::describeAllNonemptyStacks(FILE* File) {
  describeFrameStack(File);
  if (!CallingEvalStack.empty())
    describeCallingEvalStack(File);
  if (!PeekPosStack.empty())
    describePeekPosStack(File);
  if (!LoopCounterStack.empty())
    describeLoopCounterStack(File);
  if (!LocalsBaseStack.empty())
    describeLocalsStack(File);
  if (!OpcodeLocalsStack.empty())
    describeOpcodeLocalStack(File);
}

void Reader::reset() {
  Frame.reset();
  FrameStack.clear();
  PeekPos = ReadCursor();
  PeekPosStack.clear();
  LoopCounter = 0;
  LoopCounterStack.clear();
  LocalsBase = 0;
  LocalsBaseStack.clear();
  LocalValues.clear();
  OpcodeLocals.reset();
  OpcodeLocalsStack.clear();
  Output.reset();
}

void Reader::callTopLevel(Method Method, const filt::Node* Nd) {
  reset();
  Frame.reset();
  call(Method, MethodModifier::ReadAndWrite, Nd);
}

void Reader::fail() {
  TRACE_MESSAGE("method failed");
  while (!FrameStack.empty()) {
    traceExitFrame();
    popAndReturn();
  }
  reset();
  Frame.fail();
}

void Reader::fail(const std::string& Message) {
  FILE* Out = getTrace().getFile();
  if (Frame.Nd) {
    fprintf(Out, "Error: ");
    getTrace().getTextWriter()->writeAbbrev(Out, Frame.Nd);
  }
  fprintf(Out, "Error: (method %s) %s\n", getName(Frame.CallMethod),
          Message.c_str());
  Reader::fail();
}

void Reader::failBadState() {
  fail(std::string("Bad internal decompressor state in method: ") +
       getName(Frame.CallMethod));
}

void Reader::failNotImplemented() {
  fail("Method not implemented!");
}

void Reader::failCantWrite() {
  fail("Unable to write value");
}

void Reader::failFreezingEof() {
  fail("Unable to set eof on output");
}


void Reader::resume() {
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
        return failNotImplemented();
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
            LastReadValue = Input->readUint8(ReadPos);
            Output.writeUint8(LastReadValue);
            break;
          case State::Exit:
            popAndReturn();
            traceExitFrame();
            break;
          default:
            return failBadState();
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
            return failNotImplemented();
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
            return failNotImplemented();
          case OpError:  // Method::Eval
            traceEnterFrame();
            return fail("Algorithm error!");
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
              return fail("Local variable index out of range!");
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
                return failBadState();
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
                return failBadState();
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
            IntType Value = Input->readValue(ReadPos, Frame.Nd);
            if (hasReadMode())
              LastReadValue = Value;
            if (hasWriteMode()) {
              if (!Output.writeValue(LastReadValue, Frame.Nd))
                break;
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
                return failBadState();
            }
            break;
          case OpOpcode:  // Method::Eval
            return fail("Multibyte opcodes broken!");
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
                  return fail("Local variable index out of range, can't set!");
                }
                LocalValues[LocalsBase + Index] = Frame.ReturnValue;
                popAndReturn(LastReadValue);
                traceExitFrame();
                break;
              }
              default:
                return failBadState();
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
                return failBadState();
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
                    Result = isa<ByteReadStream>(Input.get());
                    break;
                  case StreamType::Int:
                    Trace.errorSexp("Stream check: ", Frame.Nd);
                    return fail("Stream check not implemented!");
                }
                break;
              case StreamKind::Output:
                switch (Stream->getStreamType()) {
                  case StreamType::Byte:
                    Result = Output.getStreamType() == StreamType::Byte;
                    break;
                  case StreamType::Int:
                    Trace.errorSexp("Stream check: ", Frame.Nd);
                    return fail("Stream check not implemented!");
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                  return fail("Unable to evaluate call");
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
                return failBadState();
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
                return failBadState();
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
            const uint32_t OldSize = Input->readBlockSize(ReadPos);
            TRACE(uint32_t, "block size", OldSize);
            Input->pushEobAddress(ReadPos, OldSize);
            if (!Output.writeAction(Symtab->getBlockEnterCallback()))
              break;
            Frame.CallState = State::Exit;
            call(DispatchedMethod, Frame.CallModifier, Frame.Nd);
            break;
          }
          case State::Exit:
            if (!Output.writeAction(Symtab->getBlockExitCallback()))
              break;
            ReadPos.popEobAddress();
            popAndReturn();
            traceExitFrame();
            break;
          default:
            return failBadState();
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
            if (CallingEvalStack.empty())
              return fail(
                  "Not inside a call frame, can't evaluate parameter "
                  "accessor!");
            assert(isa<ParamNode>(Frame.Nd));
            auto* Param = cast<ParamNode>(Frame.Nd);
            IntType ParamIndex = Param->getValue() + 1;
            if (ParamIndex >= IntType(CallingEval.Caller->getNumKids()))
              return fail("Parameter reference doesn't match callling context!");
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
            return failBadState();
        }
        break;
      case Method::GetFile:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            MagicNumber = Input->readUint32(ReadPos);
            TRACE(hex_uint32_t, "magic number", MagicNumber);
            if (MagicNumber != WasmBinaryMagic)
              return fail(
                  "Unable to compress. Did not find WASM binary magic number!");
            if (!Output.writeUint32(MagicNumber))
              break;
            Version = Input->readUint32(ReadPos);
            TRACE(hex_uint32_t, "version", Version);
            if (Version != WasmBinaryVersion)
              return fail("Unable to compress. WASM version not known");
            if (!Output.writeUint32(Version))
              break;
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
            if (!Output.writeFreezeEof())
              return failFreezingEof();
            popAndReturn();
            traceExitFrame();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::GetSecName:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            CurSectionName.clear();
            LoopCounterStack.push(Input->readVaruint32(ReadPos));
            Output.writeVaruint32(LoopCounter);
            Frame.CallState = State::Loop;
            break;
          case State::Loop: {
            if (LoopCounter == 0) {
              Frame.CallState = State::Exit;
              break;
            }
            --LoopCounter;
            uint8_t Byte = Input->readUint8(ReadPos);
            Output.writeUint8(Byte);
            CurSectionName.push_back(char(Byte));
            break;
          }
          case State::Exit:
            popAndReturn();
            traceExitFrame();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::GetSection:
        switch (Frame.CallState) {
          case State::Enter:
            traceEnterFrame();
            assert(isa<ByteReadStream>(Input.get()));
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
            popAndReturn();
            traceExitFrame();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::ReadOpcode:
        // Note: Assumes that caller pushes OpcodeLocals;
        switch (Frame.Nd->getType()) {
          default:
            return failNotImplemented();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
                return failBadState();
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
          return fail("Malformed input in compressed file");
        break;
    }
  }
#if LOG_RUNMETHODS
  TRACE_BLOCK({ describeAllNonemptyStacks(tracE.getFile()); });
  TRACE_EXIT_OVERRIDE("resume");
#endif
}

void Reader::readBackFilled() {
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

}  // end of namespace interp

}  // end of namespace wasm
