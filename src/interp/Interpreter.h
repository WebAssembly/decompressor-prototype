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

// Models the interpretater for filter s-expressions.

#ifndef DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
#define DECOMPRESSOR_SRC_INTERP_INTERPRETER_H

#include "stream/Queue.h"
#include "stream/ReadCursor.h"
#include "stream/WriteCursor.h"
#include "interp/Interpreter.def"
#include "interp/ReadStream.h"
#include "interp/TraceSexpReaderWriter.h"
#include "interp/WriteStream.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace filt {
class TextWriter;
}

namespace interp {

// Defines the algorithm interpreter for filter sections, and the
// corresponding state associated with the interpreter.
//
// TODO(karlschimpf) Rename this to a better name.
class Interpreter {
  Interpreter() = delete;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

 public:
  // TODO(kschimpf): Add Output.
  Interpreter(std::shared_ptr<decode::Queue> Input,
              std::shared_ptr<decode::Queue> Output,
              std::shared_ptr<filt::SymbolTable> Symtab);

  ~Interpreter() {}

  // Processes each section in input, and decompresses it (if applicable)
  // to the corresponding output.
  void decompress();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

 private:
  enum class Method {
#define X(tag, name) tag,
    INTERPRETER_METHODS_TABLE
#undef X
        NO_SUCH_METHOD
  };
  static const char* getName(Method M);

  enum class State {
#define X(tag, name) tag,
    INTERPRETER_STATES_TABLE
#undef X
        NO_SUCH_STATE
  };
  static const char* getName(State C);

  // The call stack of methods being applied.
  struct CallFrame {
    CallFrame() { reset(); }
    CallFrame(Method CallMethod, const filt::Node* Nd)
        : CallMethod(CallMethod), CallState(State::Enter), Nd(Nd) {}
    CallFrame(const CallFrame& M)
        : CallMethod(M.CallMethod), CallState(M.CallState), Nd(M.Nd) {}
    void reset() {
      CallMethod = Method::Finished;
      CallState = State::Succeeded;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void fail() {
      CallMethod = Method::Finished;
      CallState = State::Failed;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void describe(FILE* File, filt::TextWriter* Writer) const;
    Method CallMethod;
    State CallState;
    const filt::Node* Nd;
    // Holds return value from last called routine, except when
    // exiting.  Note: For method write, this corresponds to the value
    // to write as well.
    decode::IntType ReturnValue;
  };

  // The stack of calling "eval" expressions.
  struct EvalFrame {
    EvalFrame() { reset(); }
    EvalFrame(const filt::EvalNode* Caller, size_t CallingEvalIndex)
        : Caller(Caller), CallingEvalIndex(CallingEvalIndex) {
      assert(Caller != nullptr);
    }
    EvalFrame(const EvalFrame& F)
        : Caller(F.Caller), CallingEvalIndex(F.CallingEvalIndex) {}
    bool isDefined() const { return Caller != nullptr; }
    void reset() {
      Caller = nullptr;
      CallingEvalIndex = 0;
    }
    void describe(FILE* File, filt::TextWriter* Writer) const;
    const filt::EvalNode* Caller;
    size_t CallingEvalIndex;
  };

  decode::ReadCursor ReadPos;
  std::shared_ptr<ReadStream> Reader;
  decode::WriteCursor WritePos;
  std::shared_ptr<WriteStream> Writer;
  filt::Node* DefaultFormat;
  std::shared_ptr<filt::SymbolTable> Symtab;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  // The current section name (if applicable).
  std::string CurSectionName;
  // The last read value.
  decode::IntType LastReadValue;
  // Holds the method to call (i.e. dispatch) if code expects a method to be
  // provided by the caller.
  Method DispatchedMethod;
  bool MinimizeBlockSize;
  TraceClassSexpReaderWriter Trace;

  // The stack of called methods.
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  // The stack of (eval) calls.
  EvalFrame CallingEval;
  utils::ValueStack<EvalFrame> CallingEvalStack;
  // The stack of passed write Values.
  decode::IntType WriteValue;
  utils::ValueStack<decode::IntType> WriteValueStack;
  // The stack of read cursors (used by peek)
  decode::ReadCursor PeekPos;
  utils::ValueStack<decode::ReadCursor> PeekPosStack;
  // The stack of block patch locations.
  decode::WriteCursor BlockStart;
  utils::ValueStack<decode::WriteCursor> BlockStartStack;
  // The stack of loop counters.
  size_t LoopCounter;
  utils::ValueStack<size_t> LoopCounterStack;
  // The stack of opcode Selshift/CaseMasks.
  struct OpcodeLocalsFrame {
    uint32_t SelShift;
    decode::IntType CaseMask;
    const filt::CaseNode* Case;
    OpcodeLocalsFrame() { reset(); }
    void describe(FILE* File, filt::TextWriter* Writer = nullptr) const;
    void reset() {
      SelShift = 0;
      CaseMask = 0;
      Case = nullptr;
    }
  };
  OpcodeLocalsFrame OpcodeLocals;
  utils::ValueStack<OpcodeLocalsFrame> OpcodeLocalsStack;

  void fail();

  void fail(const char* Message);

  // Initializes all internal stacks, for an initial call to Method with
  // argument Nd.
  void callTopLevel(Method Method, const filt::Node* Nd);

  // Sets up code to call Method with argument Nd.
  void call(Method Method, const filt::Node* Nd) {
    Frame.ReturnValue = 0;
    FrameStack.push();
    Frame.CallMethod = Method;
    Frame.CallState = State::Enter;
    Frame.Nd = Nd;
    Frame.ReturnValue = 0;
  }

  // Sets up code to call write method Method with arguments Nd and WriteValue.
  // Note: Method may not be Method::Write. Rather, it may be some intermediate
  // method that sets up a call to Method::Write using field DispatchesMethod.
  void callWrite(Method Method,
                 const filt::Node* Nd,
                 decode::IntType WriteValue) {
    call(Method, Nd);
    WriteValueStack.push();
    this->WriteValue = WriteValue;
  }

  // Sets up code to return from method Write to the calling method.
  void popAndReturnWriteValue() {
    assert(Frame.CallMethod == Method::Write);
    popAndReturn(WriteValue);
    WriteValueStack.pop();
  }

  // Sets up code to return from method Read to the calling method, returning
  // Value.
  void popAndReturnReadValue(decode::IntType Value) {
    LastReadValue = Value;
    popAndReturn(Value);
  }

  TraceClassSexpReaderWriter& getTrace() { return Trace; }
  void decompressSection();

  // Stack model
  void runMethods();
  void TraceEnterFrame() {
    assert(Frame.CallState == Interpreter::State::Enter);
    TRACE_ENTER(getName(Frame.CallMethod));
    TRACE_SEXP(nullptr, Frame.Nd);
  }
  void TraceExitFrame() { TRACE_EXIT_OVERRIDE(getName(Frame.CallMethod)); }
  void popAndReturn(decode::IntType Value = 0) {
    FrameStack.pop();
    Frame.ReturnValue = Value;
    TRACE(IntType, "returns", Value);
  }

  bool hasEnoughHeadroom() const;
  bool needsMoreInput() const { return !isSuccessful() && !errorsFound(); }
  bool isFinished() const { return Frame.CallMethod == Method::Finished; }
  bool isSuccessful() const { return Frame.CallState == State::Succeeded; }
  bool errorsFound() const { return Frame.CallState == State::Failed; }

  // Fills input stream using the read backing of the input stream, so that
  // runMethods can use a push model.
  void readBackFilled();

  // For debugging only.
  void describeFrameStack(FILE* Out);
  void describeWriteValueStack(FILE* Out);
  void describeCallingEvalStack(FILE* Out);
  void describePeekPosStack(FILE* Out);
  void describeLoopCounterStack(FILE* Out);
  void describeBlockStartStack(FILE* Out);
  void describeOpcodeLocalStack(FILE* Out);
  void describeAllNonemptyStacks(FILE* Out);
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
