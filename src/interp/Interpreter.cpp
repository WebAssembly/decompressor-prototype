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

// Implements an interpreter of that reads a file and applies algorithms to the
// data in the file.

#include "interp/Interpreter.h"

#include "interp/AlgorithmSelector.h"
#include "interp/Reader.h"
#include "interp/Writer.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "utils/Casting.h"
#include "utils/Trace.h"

#define LOG_TRUE_VALUE 1
#define LOG_FALSE_VALUE 0
#define LOG_DEFAULT_VALUE LOG_FALSE_VALUE

// By default, methods resume() and readBackFilled() are not traced,
// since they are the glue between a push and pull models. Rather, they
// conceptually mimic the natural call structure. If you want to trace
// resume() and readBackFilled() as well, change this flag to 1.
#define LOG_RUNMETHODS LOG_DEFAULT_VALUE
// The following tracks runMetthods() and readBackFilled(), which run
// interpreter methods with tracing showing equivalent non-push inter
// The following turn on logging sections, functions in the decompression
// algorithm.
#define LOG_SECTIONS LOG_DEFAULT_VALUE
#define LOG_FUNCTIONS LOG_DEFAULT_VALUE

// The following two defines allows turning on tracing for the nth (zero based)
// function.
#define LOG_NUMBERED_BLOCK LOG_DEFAULT_VALUE
#define LOG_FUNCTION_NUMBER LOG_DEFAULT_VALUE

// The following shows stack contents on each iteration of resume();
#define LOG_CALLSTACKS LOG_DEFAULT_VALUE

#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
namespace {
uint32_t LogBlockCount = 0;
}  // end of anonymous namespace
#endif

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

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

struct {
  const char* Name;
  size_t Index;
} MethodModifierName[] = {
#define X(tag, flags) \
  { #tag, flags }     \
  ,
    INTERPRETER_METHOD_MODIFIERS_TABLE
#undef X
    {"NO_SUCH_METHOD_MODIFIER", 0}};

}  // end of anonymous namespace

InterpreterFlags::InterpreterFlags()
    : MacroContext(MacroDirective::Expand),
      TraceProgress(false),
      TraceIntermediateStreams(false),
      TraceAppliedAlgorithms(false) {
}

Interpreter::CallFrame::CallFrame() {
  reset();
}

Interpreter::CallFrame::CallFrame(Method CallMethod, const filt::Node* Nd)
    : CallMethod(CallMethod),
      CallState(State::Enter),
      CallModifier(MethodModifier::ReadAndWrite),
      Nd(Nd) {
}

Interpreter::CallFrame::CallFrame(const CallFrame& M)
    : CallMethod(M.CallMethod),
      CallState(M.CallState),
      CallModifier(M.CallModifier),
      Nd(M.Nd) {
}

void Interpreter::CallFrame::reset() {
  CallMethod = Method::Started;
  CallState = State::Enter;
  CallModifier = MethodModifier::ReadAndWrite;
  Nd = nullptr;
  ReturnValue = 0;
}

void Interpreter::CallFrame::fail() {
  CallMethod = Method::Finished;
  CallState = State::Failed;
  CallModifier = MethodModifier::ReadAndWrite;
  Nd = nullptr;
  ReturnValue = 0;
}

Interpreter::EvalFrame::EvalFrame() {
  reset();
}

Interpreter::EvalFrame::EvalFrame(const filt::EvalNode* Caller,
                                  size_t CallingEvalIndex)
    : Caller(Caller), CallingEvalIndex(CallingEvalIndex) {
  assert(Caller != nullptr);
}

Interpreter::EvalFrame::EvalFrame(const EvalFrame& F)
    : Caller(F.Caller), CallingEvalIndex(F.CallingEvalIndex) {
}

bool Interpreter::EvalFrame::isDefined() const {
  return Caller != nullptr;
}

void Interpreter::EvalFrame::reset() {
  Caller = nullptr;
  CallingEvalIndex = 0;
}

void Interpreter::setInput(std::shared_ptr<Reader> Value) {
  Input = Value;
  if (Trace) {
    Trace->clearContexts();
    setTrace(Trace);
  }
}

void Interpreter::setWriter(std::shared_ptr<Writer> Value) {
  Output = Value;
  if (Trace) {
    Trace->clearContexts();
    setTrace(Trace);
  }
}

void Interpreter::addSelector(std::shared_ptr<AlgorithmSelector> Selector) {
  assert(!Symtab &&
         "Can't add selectors if symbol table defined at construction");
  Selectors.push_back(Selector);
}

void Interpreter::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  getTrace().setTraceProgress(NewValue);
}

TraceContextPtr Interpreter::getTraceContext() {
  return Input->getTraceContext();
}

void Interpreter::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (Trace) {
    Input->setTrace(Trace);
    Output->setTrace(Trace);
  }
}

const char* Interpreter::getDefaultTraceName() const {
  return "Interpreter";
}

std::shared_ptr<TraceClass> Interpreter::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>(getDefaultTraceName()));
  return Trace;
}

const char* Interpreter::getName(Method M) {
  size_t Index = size_t(M);
  if (Index >= size(MethodName))
    Index = size_t(Method::NO_SUCH_METHOD);
  return MethodName[Index];
}

const char* Interpreter::getName(MethodModifier Modifier) {
  uint32_t Index = uint32_t(Modifier);
  for (size_t i = 0; i < size(MethodModifierName); ++i) {
    if (Index == MethodModifierName[i].Index)
      return MethodModifierName[i].Name;
  }
  return MethodModifierName[size(MethodModifierName) - 1].Name;
}

const char* Interpreter::getName(State S) {
  size_t Index = size_t(S);
  if (Index >= size(StateName))
    Index = size_t(State::NO_SUCH_STATE);
  return StateName[Index];
}

const char* Interpreter::getName(SectionCode Code) {
  size_t Index = size_t(Code);
  if (Index >= size(SectionCodeName))
    Index = size_t(SectionCode::NO_SUCH_SECTION_CODE);
  return SectionCodeName[Index];
}

Interpreter::Interpreter(std::shared_ptr<Reader> Input,
                         std::shared_ptr<Writer> Output,
                         const InterpreterFlags& Flags,
                         std::shared_ptr<filt::SymbolTable> Symtab)
    : Input(Input),
      Output(Output),
      Flags(Flags),
      Symtab(Symtab),
      //      LastReadValue(0),
      DispatchedMethod(Method::NO_SUCH_METHOD),
      Catch(Method::NO_SUCH_METHOD),
      CatchStack(Catch),
      CatchState(State::NO_SUCH_STATE),
      IsFatalFailure(false),
      FrameStack(Frame),
      CallingEvalStack(CallingEval),
      LoopCounter(0),
      LoopCounterStack(LoopCounter),
      LocalsBase(0),
      LocalsBaseStack(LocalsBase),
      OpcodeLocalsStack(OpcodeLocals),
      HeaderOverride(nullptr),
      FreezeEofAtExit(true) {
  init();
}

Interpreter::Interpreter(std::shared_ptr<Reader> Input,
                         std::shared_ptr<Writer> Output,
                         const InterpreterFlags& Flags)
    : Input(Input),
      Output(Output),
      Flags(Flags),
      DispatchedMethod(Method::NO_SUCH_METHOD),
      Catch(Method::NO_SUCH_METHOD),
      CatchStack(Catch),
      CatchState(State::NO_SUCH_STATE),
      IsFatalFailure(false),
      FrameStack(Frame),
      CallingEvalStack(CallingEval),
      LoopCounter(0),
      LoopCounterStack(LoopCounter),
      LocalsBase(0),
      LocalsBaseStack(LocalsBase),
      OpcodeLocalsStack(OpcodeLocals),
      HeaderOverride(nullptr),
      FreezeEofAtExit(true) {
  init();
}

void Interpreter::init() {
  LastReadValue = 0;
  CurSectionName.reserve(MaxExpectedSectionNameSize);
  FrameStack.reserve(DefaultStackSize);
  CallingEvalStack.reserve(DefaultStackSize);
  LocalsBaseStack.reserve(DefaultStackSize);
  LocalValues.reserve(DefaultStackSize * DefaultExpectedLocals);
  OpcodeLocalsStack.reserve(DefaultStackSize);
}

Interpreter::~Interpreter() {
}

void Interpreter::traceEnterFrameInternal() {
  // Note: Enclosed in TRACE_BLOCK so that g++ will not complain when
  // compiled in release mode.
  TRACE_BLOCK({
    TRACE_ENTER(getName(Frame.CallMethod));
    if (Frame.Nd)
      TRACE(node_ptr, "Nd", Frame.Nd);
    if (Frame.CallModifier != MethodModifier::ReadAndWrite)
      TRACE_MESSAGE(std::string("(") + getName(Frame.CallModifier) + ")");
  });
}

void Interpreter::CallFrame::describe(FILE* File, TextWriter* Writer) const {
  fprintf(File, "%s.%s (%s) = ", getName(CallMethod), getName(CallState),
          getName(CallModifier));
  fprint_IntType(File, ReturnValue);
  fputs(": ", File);
  if (Nd)
    Writer->writeAbbrev(File, Nd);
  else
    fprintf(File, "nullptr\n");
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

void Interpreter::describeFrameStack(FILE* File) {
  fprintf(File, "*** Frame Stack ***\n");
  TextWriter Writer;
  for (auto& Frame : FrameStack)
    Frame.describe(File, &Writer);
  fprintf(File, "*******************\n");
}

void Interpreter::describeCallingEvalStack(FILE* File) {
  fprintf(File, "*** Eval Call Stack ****\n");
  TextWriter Writer;
  for (const auto& Frame : CallingEvalStack.iterRange(1))
    Frame.describe(File, &Writer);
  fprintf(File, "************************\n");
}

void Interpreter::describePeekPosStack(FILE* Out) {
  Input->describePeekPosStack(Out);
}

void Interpreter::describeLoopCounterStack(FILE* File) {
  fprintf(File, "*** Loop Counter Stack ***\n");
  for (const auto& Count : LoopCounterStack.iterRange(1))
    fprintf(File, "%" PRIxMAX "\n", uintmax_t(Count));
  fprintf(File, "**************************\n");
}

void Interpreter::describeLocalsStack(FILE* File) {
  fprintf(File, "*** Locals Base Stack ***\n");
  size_t BaseIndex = 0;
  for (const auto& Index : LocalsBaseStack.iterRange(1)) {
    fprintf(File, "%" PRIuMAX ":\n", uintmax_t(Index));
    for (size_t i = BaseIndex; i < Index; ++i) {
      fputs("  ", File);
      fprint_IntType(File, LocalValues[i]);
      fputc('\n', File);
    }
    BaseIndex = Index;
  }
  fprintf(File, "*************************\n");
}

void Interpreter::describeOpcodeLocalsStack(FILE* File) {
  fprintf(File, "*** Opcode Stack ***\n");
  TextWriter Writer;
  for (auto& Frame : OpcodeLocalsStack.iterRange(1))
    Frame.describe(File, &Writer);
  fprintf(File, "********************\n");
}

void Interpreter::describeState(FILE* File) {
  describeFrameStack(File);
  if (!CallingEvalStack.empty())
    describeCallingEvalStack(File);
  describePeekPosStack(File);
  if (!LoopCounterStack.empty())
    describeLoopCounterStack(File);
  if (!LocalsBaseStack.empty())
    describeLocalsStack(File);
  if (!OpcodeLocalsStack.empty())
    describeOpcodeLocalsStack(File);
  Output->describeState(File);
}

void Interpreter::traceEnterFrame() {
  assert(Frame.CallState == State::Enter);
  TRACE_BLOCK(traceEnterFrameInternal(););
}

void Interpreter::traceExitFrame() {
  TRACE_EXIT_OVERRIDE(getName(Frame.CallMethod));
}

void Interpreter::reset() {
  Frame.reset();
  FrameStack.clear();
  LoopCounter = 0;
  LoopCounterStack.clear();
  LocalsBase = 0;
  LocalsBaseStack.clear();
  LocalValues.clear();
  OpcodeLocals.reset();
  OpcodeLocalsStack.clear();
  Input->reset();
  Output->reset();
}

void Interpreter::call(Method Method,
                       MethodModifier Modifier,
                       const filt::Node* Nd) {
  Frame.ReturnValue = 0;
  FrameStack.push();
  Frame.CallMethod = Method;
  Frame.CallState = State::Enter;
  Frame.CallModifier = Modifier;
  Frame.Nd = Nd;
  Frame.ReturnValue = 0;
  traceEnterFrame();
}

void Interpreter::popAndReturn(decode::IntType Value) {
  TRACE(IntType, "returns", Value);
  traceExitFrame();
  if (!FrameStack.empty())
    FrameStack.pop();
  Frame.ReturnValue = Value;
}

void Interpreter::callTopLevel(Method Method, const filt::Node* Nd) {
  reset();
  Frame.reset();
  call(Method, MethodModifier::ReadAndWrite, Nd);
}

void Interpreter::catchOrElseFail() {
  TRACE_MESSAGE("method failed");
  TRACE(string, "Catch method", getName(Catch));
  TRACE(string, "Catch state", getName(CatchState));
  while (!FrameStack.empty()) {
    if (!IsFatalFailure && Frame.CallMethod == Catch) {
      CatchState = Frame.CallState;
      TRACE(string, "Catch state", getName(CatchState));
      Frame.CallState = State::Catch;
      if (!CatchStack.empty())
        CatchStack.pop();
      return;
    }
    popAndReturn();
  }
  reset();
  Frame.fail();
}

void Interpreter::rethrow() {
  return throwMessage(RethrowMessage);
}

void Interpreter::fail(const std::string& Message) {
  IsFatalFailure = true;
  throwMessage(Message);
}

bool Interpreter::canCatchThrow() {
  if (!IsFatalFailure) {
    for (auto CallFrame : FrameStack.riterRange()) {
      if (CallFrame.CallMethod == Catch)
        return true;
    }
  }
  return false;
}

void Interpreter::throwMessage(const std::string& Message) {
  TRACE_MESSAGE(Message);
  RethrowMessage = Message;
  if (!canCatchThrow()) {
    // Fail not throw, show context.
    TextWriter Writer;
    for (const auto& F : FrameStack.riterRange(1)) {
      fprintf(stderr, "In: ");
      Writer.writeAbbrev(stderr, F.Nd);
    }
    fprintf(stderr, "Error: (method %s) %s\n", getName(Frame.CallMethod),
            Message.c_str());
  }
  catchOrElseFail();
}

void Interpreter::throwMessage(const std::string& Message, IntType Value) {
  TRACE_MESSAGE(Message);
  RethrowMessage = Message;
  if (!canCatchThrow()) {
    // Fail not throw, show context.
    TextWriter Writer;
    for (const auto& F : FrameStack.riterRange(1)) {
      fprintf(stderr, "In: ");
      Writer.writeAbbrev(stderr, F.Nd);
    }
    fprintf(stderr, "Error: (method %s) %s%" PRIiMAX "\n",
            getName(Frame.CallMethod), Message.c_str(), uintmax_t(Value));
  }
  catchOrElseFail();
}

void Interpreter::failBadState() {
  fail(std::string("Bad internal decompressor state: ") +
       getName(Frame.CallState));
}

void Interpreter::failNotImplemented() {
  fail("Method not implemented!");
}

void Interpreter::throwCantRead() {
  throwMessage("Unable to read value");
}

void Interpreter::throwCantWrite() {
  fail("Unable to write value");
}

void Interpreter::throwCantFreezeEof() {
  fail("Unable to set eof on output");
}

void Interpreter::throwCantWriteInWriteOnlyMode() {
  fail("Method can only be processed in read mode");
}

void Interpreter::throwBadHeaderValue(IntType WantedValue,
                                      IntType FoundValue,
                                      ValueFormat Format) {
  throwMessage("Wanted header value " + std::to_string(WantedValue) +
               " but found " + std::to_string(FoundValue));
}

void Interpreter::handleOtherMethods() {
  while (1) {
    switch (Frame.CallMethod) {
      default:
        return failNotImplemented();
      case Method::Started:
        // If reached, we finished processing the input.
        assert(FrameStack.empty());
        Frame.CallMethod = Method::Finished;
        if (Input->processedInputCorrectly())
          Frame.CallState = State::Succeeded;
        else
          return throwMessage("Malformed input in compressed file");
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
        TRACE_BLOCK({ describeState(tracE.getFile()); });
        TRACE_EXIT_OVERRIDE("resume");
#endif
        return;
    }
  }
}

std::shared_ptr<SymbolTable> Interpreter::getDefaultAlgorithm(
    const Node* Header) {
  for (std::shared_ptr<AlgorithmSelector>& Sel : Selectors) {
    std::shared_ptr<SymbolTable> Symtab = Sel->getSymtab();
    const Node* Target = Symtab->getReadHeader();
    if (Header == Target)
      // In same algorithm, ignore.
      continue;
    if (*Header == *Target)
      return Symtab;
  }
  return std::shared_ptr<SymbolTable>();
}

void Interpreter::algorithmStart() {
  if (Symtab.get() != nullptr)
    return callTopLevel(Method::GetFile, nullptr);
  assert(!Selectors.empty());
  // TODO(karlschimpf) Find correct symbol table to use.
  callTopLevel(Method::GetAlgorithm, nullptr);
}

void Interpreter::algorithmRead() {
  algorithmStart();
  algorithmReadBackFilled();
}

void Interpreter::algorithmResume() {
// TODO(karlschimpf) Add catches for methods that modify local statcks, so
// that state is correctly cleaned up on a throw.
#if LOG_RUNMETHODS
  TRACE_METHOD("resume");
  TRACE_BLOCK({ describeState(tracE.getFile()); });
#endif
  if (!Input->canProcessMoreInputNow())
    return;
  while (Input->stillMoreInputToProcessNow()) {
    if (errorsFound())
      break;
#if LOG_CALLSTACKS
    TRACE_BLOCK({ describeState(tracE.getFile()); });
#endif
    switch (Frame.CallMethod) {
      default:
        return handleOtherMethods();
      case Method::CopyBlock:
        switch (Frame.CallState) {
          case State::Enter:
            Frame.CallState = State::Loop;
            break;
          case State::Loop:
            if (Input->atInputEob()) {
              Frame.CallState = State::Exit;
              break;
            }
            LastReadValue = Input->readUint8();
            Output->writeUint8(LastReadValue);
            break;
          case State::Exit:
            popAndReturn();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::Eval:
        switch (Frame.Nd->getType()) {
          case NO_SUCH_NODETYPE:
          case OpSymbolDefn:
          case OpIntLookup:
          case OpBinaryEvalBits:
          case OpBinaryAccept:
          case OpBinarySelect:
          case OpParams:
          case OpLastSymbolIs:
          case OpLiteralActionBase:
          case OpLiteralActionDef:
          case OpLiteralDef:
          case OpFile:
          case OpLocals:
          case OpRename:
          case OpSymbol:
          case OpSection:
          case OpUndefine:
          case OpUnknownSection:  // Method::Eval
            return failNotImplemented();
          case OpError:  // Method::Eval
            return throwMessage("Algorithm error!");
          case OpFileHeader:
            switch (Frame.CallState) {
              case State::Enter:
                if (!CatchStack.empty()) {
                  CatchStack.push(Method::Eval);
                }
                LoopCounterStack.push(0);
                Frame.CallState = State::Loop;
                break;
              case State::Loop: {
                if (LoopCounter == size_t(Frame.Nd->getNumKids())) {
                  Frame.CallState = State::Exit;
                  break;
                }
                auto Lit =
                    dyn_cast<IntegerNode>(Frame.Nd->getKid(LoopCounter++));
                TRACE(node_ptr, "Lit", Lit);
                if (Lit == nullptr)
                  return throwMessage(
                      "Literal header value expected, but not found");
                IntType WantedValue = Lit->getValue();
                if (!Lit->definesIntTypeFormat())
                  return throwMessage(
                      "Format header contains badly formed constant");
                IntTypeFormat TypeFormat = Lit->getIntTypeFormat();
                IntType FoundValue;
                if (!Input->readHeaderValue(TypeFormat, FoundValue)) {
                  TRACE(IntType, "Found", FoundValue);
                  return throwMessage("Unable to read header value");
                }
                if (errorsFound())
                  return;
                if (WantedValue != FoundValue)
                  return throwBadHeaderValue(WantedValue, FoundValue,
                                             Lit->getFormat());
                if (hasWriteMode())
                  Output->writeHeaderValue(FoundValue, TypeFormat);
                break;
              }
              case State::Catch:
                LoopCounterStack.pop();
                popAndReturn();
                return rethrow();
              case State::Exit:
                if (!CatchStack.empty())
                  CatchStack.pop();
                LoopCounterStack.pop();
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpBitwiseAnd:
            switch (Frame.CallState) {
              case State::Enter:
                if (!hasReadMode())
                  return throwCantWriteInWriteOnlyMode();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                LocalValues.push_back(Frame.ReturnValue);
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                IntType Arg2 = Frame.ReturnValue;
                IntType Arg1 = LocalValues.back();
                LocalValues.pop_back();
                popAndReturn(Arg1 & Arg2);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpBitwiseOr:
            switch (Frame.CallState) {
              case State::Enter:
                if (!hasReadMode())
                  return throwCantWriteInWriteOnlyMode();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                LocalValues.push_back(Frame.ReturnValue);
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                IntType Arg2 = Frame.ReturnValue;
                IntType Arg1 = LocalValues.back();
                LocalValues.pop_back();
                popAndReturn(Arg1 | Arg2);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpBitwiseXor:
            switch (Frame.CallState) {
              case State::Enter:
                if (!hasReadMode())
                  return throwCantWriteInWriteOnlyMode();
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                LocalValues.push_back(Frame.ReturnValue);
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                IntType Arg2 = Frame.ReturnValue;
                IntType Arg1 = LocalValues.back();
                LocalValues.pop_back();
                popAndReturn(Arg1 ^ Arg2);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpBitwiseNegate:
            switch (Frame.CallState) {
              case State::Enter:
                if (!hasReadMode())
                  return throwCantWriteInWriteOnlyMode();
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Exit: {
                IntType Arg = Frame.ReturnValue;
                LocalValues.pop_back();
                popAndReturn(~Arg);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpCallback: {  // Method::Eval
            IntType Action =
                cast<CallbackNode>(Frame.Nd)->getValue()->getValue();
            if (!Input->readAction(Action) || !Output->writeAction(Action))
              return throwMessage("Unable to apply action: ", Action);
            popAndReturn(LastReadValue);
            break;
          }
          case OpI32Const:
          case OpI64Const:
          case OpU8Const:
          case OpU32Const:
          case OpU64Const: {  // Method::Eval
            IntType Value = dyn_cast<IntegerNode>(Frame.Nd)->getValue();
            if (hasReadMode())
              LastReadValue = Value;
            popAndReturn(Value);
            break;
          }
          case OpLastRead:
            popAndReturn(LastReadValue);
            break;
          case OpLocal: {
            const auto* Local = dyn_cast<LocalNode>(Frame.Nd);
            size_t Index = Local->getValue();
            if (LocalsBase + Index >= LocalValues.size()) {
              return throwMessage("Local variable index out of range!");
            }
            popAndReturn(LocalValues[LocalsBase + Index]);
            break;
          }
          case OpPeek:
            switch (Frame.CallState) {
              case State::Enter:
                if (!Input->pushPeekPos())
                  return failBadState();
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Exit:
                if (!Input->popPeekPos())
                  return failBadState();
                popAndReturn(Frame.ReturnValue);
                break;
              default:
                return failBadState();
            }
            break;
          case OpRead:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn(Frame.ReturnValue);
                break;
              default:
                return failBadState();
            }
            break;
          case OpBit:
          case OpUint32:
          case OpUint64:
          case OpUint8:
          case OpVarint32:
          case OpVarint64:
          case OpVaruint32:
          case OpVaruint64: {
            if (hasReadMode())
              if (!Input->readValue(Frame.Nd, LastReadValue))
                return throwCantRead();
            if (hasWriteMode()) {
              if (!Output->writeValue(LastReadValue, Frame.Nd))
                return throwCantWrite();
            }
            popAndReturn(LastReadValue);
            break;
          }
          case OpBinaryEval:
            if (hasReadMode())
              if (!Input->readBinary(Frame.Nd, LastReadValue))
                return throwCantRead();
            if (hasWriteMode())
              if (!Output->writeBinary(LastReadValue, Frame.Nd))
                return throwCantWrite();
            popAndReturn(LastReadValue);
            break;
          case OpMap:
            switch (Frame.CallState) {
              case State::Enter:
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpOpcode:  // Method::Eval
            return throwMessage("Multibyte opcodes broken!");
          case OpSet:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                const auto* Local = dyn_cast<LocalNode>(Frame.Nd->getKid(0));
                size_t Index = Local->getValue();
                if (LocalsBase + Index >= LocalValues.size()) {
                  return throwMessage(
                      "Local variable index out of range, can't set!");
                }
                LocalValues[LocalsBase + Index] = Frame.ReturnValue;
                popAndReturn(LastReadValue);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpWrite:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                LoopCounterStack.push(0);
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (++LoopCounter == size_t(Frame.Nd->getNumKids())) {
                  Frame.CallState = State::Exit;
                  break;
                }
                Frame.CallState = State::Step2;
                call(Method::Eval, MethodModifier::ReadOnly,
                     Frame.Nd->getKid(LoopCounter));
                break;
              case State::Step2:
                Frame.CallState = State::Loop;
                call(Method::Eval, MethodModifier::WriteOnly,
                     Frame.Nd->getKid(0));
                break;
              case State::Exit:
                LoopCounterStack.pop();
                popAndReturn(Frame.ReturnValue);
                break;
              default:
                return failBadState();
            }
            break;
          case OpNot:  // Method::Eval
            if (!hasReadMode())
              return throwCantWriteInWriteOnlyMode();
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpAnd:  // Method::Eval
            if (!hasReadMode())
              return throwCantWriteInWriteOnlyMode();
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpOr:  // Method::Eval
            if (!hasReadMode())
              return throwCantWriteInWriteOnlyMode();
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue == 0)
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit: {
                popAndReturn(Frame.ReturnValue);
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpSequence:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpTable:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                switch (Flags.MacroContext) {
                  case MacroDirective::Expand:
                    if (hasReadMode())
                      if (!Input->tablePush(Frame.ReturnValue))
                        return throwCantRead();
                    break;
                  case MacroDirective::Contract:
                    if (hasWriteMode())
                      if (!Output->tablePush(Frame.ReturnValue))
                        return throwCantWrite();
                    break;
                }
                Frame.CallState = State::Step3;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                switch (Flags.MacroContext) {
                  case MacroDirective::Expand:
                    if (hasReadMode())
                      if (!Input->tablePop())
                        return throwCantRead();
                    break;
                  case MacroDirective::Contract:
                    if (hasWriteMode())
                      if (!Output->tablePop())
                        return throwCantWrite();
                    break;
                }
                popAndReturn(LastReadValue);
                break;
              default:
                return failBadState();
            }
            break;
          case OpLoop:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpLoopUnbounded:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Loop;
                break;
              case State::Loop:
                if (Input->atInputEob()) {
                  Frame.CallState = State::Exit;
                  break;
                }
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpIfThen:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
                break;
              case State::Step2:
                Frame.CallState = State::Exit;
                if (Frame.ReturnValue != 0)
                  call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpIfThenElse:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpSwitch:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Step2;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(0));
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpCase:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Frame.Nd->getKid(1));
                break;
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpDefine:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                const DefineNode* Define = cast<DefineNode>(Frame.Nd);
                if (size_t NumLocals = Define->getNumLocals()) {
                  LocalsBaseStack.push(LocalValues.size());
                  for (size_t i = 0; i < NumLocals; ++i)
                    LocalValues.push_back(0);
                }
                Frame.CallState = State::Exit;
                call(Method::Eval, Frame.CallModifier, Define->getBody());
                break;
              }
              case State::Exit: {
                const DefineNode* Define = cast<DefineNode>(Frame.Nd);
                if (Define->getNumLocals()) {
                  while (LocalValues.size() > LocalsBase)
                    LocalValues.pop_back();
                  LocalsBaseStack.pop();
                }
                popAndReturn();
                break;
              }
              default:
                return failBadState();
            }
            break;
          case OpParam:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                DispatchedMethod = Method::Eval;
                call(Method::EvalParam, Frame.CallModifier, Frame.Nd);
                break;
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpLiteralActionUse:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                Frame.CallState = State::Exit;
                auto* Sym = dyn_cast<SymbolNode>(Frame.Nd->getKid(0));
                assert(Sym);
                // Note: To handle local algorithm overrides (when processing
                // code in an enclosing scope) we need to get the definition
                // from the current algorithm, not the algorithm the symbol was
                // defined in.
                const LiteralActionDefNode* Defn =
                    Symtab->getSymbolDefn(Sym)->getLiteralActionDefinition();
                if (Defn == nullptr) {
                  fprintf(stderr, "Eval can't find literal action: %s\n",
                          Sym->getName().c_str());
                  return throwMessage("Unable to evaluate literal action");
                }
                call(Method::Eval, Frame.CallModifier, Defn);
                break;
              }
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpLiteralUse:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                Frame.CallState = State::Exit;
                auto* Sym = dyn_cast<SymbolNode>(Frame.Nd->getKid(0));
                assert(Sym);
                // Note: To handle local algorithm overrides (when processing
                // code in an enclosing scope) we need to get the definition
                // from the current algorithm, not the algorithm the symbol was
                // defined in.
                const LiteralDefNode* Defn =
                    Symtab->getSymbolDefn(Sym)->getLiteralDefinition();
                if (Defn == nullptr) {
                  fprintf(stderr, "Eval can't find literal: %s\n",
                          Sym->getName().c_str());
                  return throwMessage("Unable to evaluate literal");
                }
                call(Method::Eval, Frame.CallModifier, Defn);
                break;
              }
              case State::Exit:
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpEval:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter: {
                auto* Sym = dyn_cast<SymbolNode>(Frame.Nd->getKid(0));
                assert(Sym);
                // Note: To handle local algorithm overrides (when processing
                // code in an enclosing scope) we need to get the definition
                // from the current algorithm, not the algorithm the symbol was
                // defined in.
                const DefineNode* Defn =
                    Symtab->getSymbolDefn(Sym)->getDefineDefinition();
                if (Defn == nullptr) {
                  fprintf(stderr, "Eval can't find definition: %s\n",
                          Sym->getName().c_str());
                  return throwMessage("Unable to evaluate call");
                }
                auto* NumParams = dyn_cast<ParamsNode>(Defn->getKid(1));
                assert(NumParams);
                int NumCallArgs = Frame.Nd->getNumKids() - 1;
                if (NumParams->getValue() != IntType(NumCallArgs)) {
                  fprintf(stderr, "Definition %s expects %" PRIuMAX
                                  "parameters, found: %" PRIuMAX "\n",
                          Sym->getName().c_str(),
                          uintmax_t(NumParams->getValue()),
                          uintmax_t(NumCallArgs));
                  return throwMessage("Unable to evaluate call");
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
                break;
              default:
                return failBadState();
            }
            break;
          case OpBlock:  // Method::Eval
            switch (Frame.CallState) {
              case State::Enter:
#if LOG_FUNCTIONS || LOG_NUMBERED_BLOCK
                // NOTE: This assumes that blocks (outside of sections) are only
                // used to define functions.
                TRACE_BLOCK({
                  fprintf(getTrace().indent(), " Function %" PRIuMAX "\n",
                          uintmax_t(LogBlockCount));
                  if (LOG_NUMBERED_BLOCK &&
                      LogBlockCount == LOG_FUNCTION_NUMBER)
                    getTrace().setTraceProgress(true);
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
                    getTrace().setTraceProgress(0);
                });
#endif
                ++LogBlockCount;
#endif
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpVoid:  // Method::Eval
            popAndReturn(LastReadValue);
            break;
        }
        break;
      case Method::EvalBlock:
        switch (Frame.CallState) {
          case State::Enter: {
            IntType EnterBlock = IntType(PredefinedSymbol::Block_enter);
            if (!Input->readAction(EnterBlock) ||
                !Output->writeAction(EnterBlock))
              return fatal("Unable to enter block");
            Frame.CallState = State::Exit;
            call(DispatchedMethod, Frame.CallModifier, Frame.Nd);
            break;
          }
          case State::Exit: {
            IntType ExitBlock = IntType(PredefinedSymbol::Block_exit);
            if (!Input->readAction(ExitBlock) ||
                !Output->writeAction(ExitBlock))
              return fatal("unable to close block");
            popAndReturn();
            break;
          }
          default:
            return failBadState();
        }
        break;
      case Method::EvalParam:
        switch (Frame.CallState) {
          case State::Enter: {
            if (CallingEvalStack.empty())
              return throwMessage(
                  "Not inside a call frame, can't evaluate parameter "
                  "accessor!");
            assert(isa<ParamNode>(Frame.Nd));
            auto* Param = cast<ParamNode>(Frame.Nd);
            IntType ParamIndex = Param->getValue() + 1;
            if (ParamIndex >= IntType(CallingEval.Caller->getNumKids()))
              return throwMessage(
                  "Parameter reference doesn't match callling context!");
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
            break;
          default:
            return failBadState();
        }
        break;
      case Method::GetAlgorithm:
        TRACE(string, "GetAlgorithm state", getName(Frame.CallState));
        switch (Frame.CallState) {
          case State::Enter:
            assert(CatchStack.empty());
            assert(LoopCounterStack.empty());
            CatchStack.push(Method::GetAlgorithm);
            if (!Input->pushPeekPos())
              return failBadState();
            LoopCounterStack.push(0);
            Frame.CallState = State::Loop;
            break;
          case State::Loop:
            assert(CatchStack.size() == 1);
            assert(LoopCounterStack.size() == 1);
            if (LoopCounter >= Selectors.size()) {
              CatchStack.pop();
              if (!Input->popPeekPos())
                return failBadState();
              LoopCounterStack.pop();
              return throwMessage("Unable to find algorithm to apply!");
            }
            Frame.CallState = State::Step2;
            call(Method::Eval, MethodModifier::ReadOnly,
                 Selectors[LoopCounter]->getSymtab()->getReadHeader());
            break;
          case State::Step2:
            assert(CatchStack.size() == 1);
            assert(LoopCounterStack.size() == 1);
            // Found algorithm. Install and then use.
            CatchStack.pop();
            if (!Input->popPeekPos())
              return failBadState();
            TRACE(size_t, "Select counter", LoopCounter);
            if (!Selectors[LoopCounter]->configure(this))
              return fail("Problems configuring reader for found header");
            if (!Symtab)
              return fail("No algorithm defined for selected algorithm!");
            Frame.CallState = State::Step3;
            break;
          case State::Step3:
            assert(CatchStack.empty());
            assert(LoopCounterStack.size() == 1);
            Frame.CallState = State::Step4;
            if (Flags.TraceAppliedAlgorithms) {
              fprintf(stderr, "Applying Algorithm:\n");
              TextWriter Writer;
              (++Writer).write(stderr, Symtab.get());
            }
            call(Method::GetFile, Frame.CallModifier, Frame.Nd);
            break;
          case State::Step4:
            assert(CatchStack.empty());
            assert(LoopCounterStack.size() == 1);
            // Parsed data associated with algorithm. Now process rest of input.
            TRACE(size_t, "Select counter", LoopCounter);
            if (Selectors[LoopCounter]->reset(this)) {
              if (Symtab) {
                TRACE_MESSAGE("Reset with symtab");
                // Defined a symbol table to apply next, so process it without
                // changing the selector.
                Frame.CallState = State::Step3;
                break;
              }
            } else
              return throwMessage(
                  "Unable to reset state after appplying algorithm");
            TRACE_MESSAGE("Reset did not specify any more symtabs");
            if (Input->atInputEob()) {
              LoopCounterStack.pop();
              Frame.CallState = State::Exit;
              break;
            }
            CatchStack.push(Method::GetAlgorithm);
            if (!Input->pushPeekPos())
              return failBadState();
            LoopCounter = 0;
            Frame.CallState = State::Loop;
            break;
          case State::Catch:
            assert(CatchStack.empty());
            switch (CatchState) {
              default:
                return rethrow();
              case State::Step2:
                assert(LoopCounterStack.size() == 1);
                CatchStack.push(Method::GetAlgorithm);
                if (!(Input->popPeekPos() && Input->pushPeekPos()))
                  return failBadState();
                LoopCounter++;
                Frame.CallState = State::Loop;
                break;
            }
            break;
          case State::Exit:
            assert(CatchStack.empty());
            assert(LoopCounterStack.empty());
            popAndReturn();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::GetFile:
        switch (Frame.CallState) {
          case State::Enter: {
            if (Frame.Nd == nullptr) {
              Frame.Nd = Symtab->getInstalledRoot();
              assert(Frame.Nd);
              assert(isa<FileNode>(Frame.Nd));
            }
            // TODO: Separate out read and write headers, since they may
            // be different.
            const Node* Header =
                HeaderOverride ? HeaderOverride : Symtab->getReadHeader();
            if (!isa<FileHeaderNode>(Header))
              return fail("Can't find matching header definition");
            Frame.CallState = State::Step2;
            call(Method::Eval, Frame.CallModifier, Header);
            break;
          }
          case State::Step2: {
            Frame.CallState = State::Exit;
            if (!Output->writeHeaderClose())
              return fail("Unable to write header");
            SymbolNode* File = Symtab->getPredefined(PredefinedSymbol::File);
            if (File == nullptr)
              throwMessage("Can't find sexpression to process file");
            const Node* FileDefn = File->getDefineDefinition();
            if (FileDefn == nullptr)
              throwMessage("Can't find sexpression to process file");
            call(Method::Eval, Frame.CallModifier, FileDefn);
            break;
          }
          case State::Exit:
            if (FreezeEofAtExit && !Output->writeFreezeEof())
              return throwCantFreezeEof();
            popAndReturn();
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
                break;
              default:
                return failBadState();
            }
          case OpUint8:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = CHAR_BIT;
                popAndReturn();
                break;
              default:
                return failBadState();
            }
          case OpUint32:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = CHAR_BIT;
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
          case OpUint64:  // Method::ReadOpcode
            switch (Frame.CallState) {
              case State::Enter:
                Frame.CallState = State::Exit;
                call(Method::Eval, MethodModifier::ReadOnly, Frame.Nd);
                break;
              case State::Exit:
                OpcodeLocals.CaseMask = Frame.ReturnValue;
                OpcodeLocals.SelShift = CHAR_BIT;
                popAndReturn();
                break;
              default:
                return failBadState();
            }
            break;
        }
        break;
    }
  }
#if LOG_RUNMETHODS
  TRACE_BLOCK({ describeState(tracE.getFile()); });
  TRACE_EXIT_OVERRIDE("resume");
#endif
}

void Interpreter::algorithmReadBackFilled() {
#if LOG_RUNMETHODS
  TRACE_METHOD("readBackFilled");
#endif
  Input->readFillStart();
  while (!isFinished()) {
    Input->readFillMoreInput();
    algorithmResume();
  }
}

}  // end of namespace interp

}  // end of namespace wasm
