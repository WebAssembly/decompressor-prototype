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

Reader::Reader(std::shared_ptr<decode::Queue> Input,
               std::shared_ptr<filt::SymbolTable> Symtab,
               TraceClassSexp& Trace)
    : ReadPos(StreamType::Byte, Input),
      InputReader(std::make_shared<ByteReadStream>()),
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

void Reader::clearFrameStack() {
  Frame.reset();
  FrameStack.clear();
}

void Reader::clearStacksExceptFrame() {
  PeekPos = ReadCursor();
  PeekPosStack.clear();
  LoopCounter = 0;
  LoopCounterStack.clear();
  LocalsBase = 0;
  LocalsBaseStack.clear();
  LocalValues.clear();
  OpcodeLocals.reset();
  OpcodeLocalsStack.clear();
}

bool Reader::writeUint8(uint8_t) {
  return true;
}

bool Reader::writeUint32(uint32_t) {
  return true;
}

bool Reader::writeUint64(uint64_t) {
  return true;
}

bool Reader::writeVarint32(int32_t) {
  return true;
}

bool Reader::writeVarint64(int64_t) {
  return true;
}

bool Reader::writeVaruint32(uint32_t) {
  return true;
}

bool Reader::writeVaruint64(uint64_t) {
  return true;
}

bool Reader::writeValue(IntType Value, const Node* Format) {
  return true;
}

bool Reader::writeAction(const CallbackNode* Action) {
  return true;
}

void Reader::callTopLevel(Method Method, const filt::Node* Nd) {
  clearFrameStack();
  clearStacksExceptFrame();
  call(Method, MethodModifier::ReadAndWrite, Nd);
}

void Reader::fail() {
  TRACE_MESSAGE("method failed");
  while (!FrameStack.empty()) {
    traceExitFrame();
    popAndReturn();
  }
  clearStacksExceptFrame();
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

}  // end of namespace interp

}  // end of namespace wasm
