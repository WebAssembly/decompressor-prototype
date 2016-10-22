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

// Defines a (stream) reader for wasm/casm files.

#ifndef DECOMPRESSOR_SRC_INTERP_READER_H
#define DECOMPRESSOR_SRC_INTERP_READER_H

#include "stream/Queue.h"
#include "stream/ReadCursor.h"
#include "interp/Interpreter.def"
#include "interp/ReadStream.h"
#include "interp/Writer.h"
#include "sexp/TraceSexp.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace interp {

class Reader {
  Reader() = delete;
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

 public:
  Reader(std::shared_ptr<decode::Queue> Input,
         Writer& Output,
         std::shared_ptr<filt::SymbolTable> Symtab,
         filt::TraceClassSexp& Trace);
  ~Reader() {}

  // Starts up decompression.
  void start() { callTopLevel(Method::GetFile, nullptr); }

  // Resumes decompression where it left off. Assumes that more
  // input has been added since the previous start()/resume() call.
  // Resume should be called until isFinished() is true.
  void resume();

  // Reads from backfilled input stream.
  void readBackFilled();

  // Check status of read.
  bool isFinished() const { return Frame.CallMethod == Method::Finished; }
  bool isSuccessful() const { return Frame.CallState == State::Succeeded; }
  bool errorsFound() const { return Frame.CallState == State::Failed; }
  bool hasReadMode() const { return isReadModifier(Frame.CallModifier); }
  bool hasWriteMode() const { return isWriteModifier(Frame.CallModifier); }

  // Force interpretation to fail.
  void fail(const std::string& Message);

  virtual decode::ReadCursor& getPos();

  filt::TraceClassSexp& getTrace() { return Trace; }

  enum class SectionCode : uint32_t {
#define X(code, value) code = value,
    SECTION_CODES_TABLE
#undef X
        NO_SUCH_SECTION_CODE
  };

  static const char* getName(SectionCode Code);

 protected:
  enum class Method {
#define X(tag) tag,
    INTERPRETER_METHODS_TABLE
#undef X
        NO_SUCH_METHOD
  };
  static const char* getName(Method M);

  enum class MethodModifier {
#define X(tag, flags) tag = flags,
    INTERPRETER_METHOD_MODIFIERS_TABLE
#undef X
        NO_SUCH_METHOD_MODIFIER
  };
  static const char* getName(MethodModifier Modifier);
  static bool isReadModifier(MethodModifier Modifier) {
    return uint32_t(Modifier) & 0x1;
  }
  static bool isWriteModifier(MethodModifier Modifier) {
    return uint32_t(Modifier) & 0x2;
  }

  enum class State {
#define X(tag) tag,
    INTERPRETER_STATES_TABLE
#undef X
        NO_SUCH_STATE
  };
  static const char* getName(State C);

  // The call stack of methods being applied.
  struct CallFrame {
    CallFrame() { reset(); }
    CallFrame(Method CallMethod, const filt::Node* Nd)
        : CallMethod(CallMethod),
          CallState(State::Enter),
          CallModifier(MethodModifier::ReadAndWrite),
          Nd(Nd) {}
    CallFrame(const CallFrame& M)
        : CallMethod(M.CallMethod),
          CallState(M.CallState),
          CallModifier(M.CallModifier),
          Nd(M.Nd) {}
    void reset() {
      CallMethod = Method::Started;
      CallState = State::Enter;
      CallModifier = MethodModifier::ReadAndWrite;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void fail() {
      CallMethod = Method::Finished;
      CallState = State::Failed;
      CallModifier = MethodModifier::ReadAndWrite;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void describe(FILE* File, filt::TextWriter* Writer) const;
    Method CallMethod;
    State CallState;
    MethodModifier CallModifier;
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
  std::shared_ptr<ReadStream> Input;
  Writer& Output;
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
  filt::TraceClassSexp& Trace;

  // The stack of called methods.
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  // The stack of (eval) calls.
  EvalFrame CallingEval;
  utils::ValueStack<EvalFrame> CallingEvalStack;
  // The stack of read cursors (used by peek)
  decode::ReadCursor PeekPos;
  utils::ValueStack<decode::ReadCursor> PeekPosStack;
  // The stack of loop counters.
  size_t LoopCounter;
  utils::ValueStack<size_t> LoopCounterStack;
  // The stack of local values.
  size_t LocalsBase;
  utils::ValueStack<size_t> LocalsBaseStack;
  std::vector<decode::IntType> LocalValues;
  // The stack of opcode Selshift/CaseMasks for multi-byte opcodes.
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

  virtual void reset();

  // Initializes all internal stacks, for an initial call to Method with
  // argument Nd.
  void callTopLevel(Method Method, const filt::Node* Nd);

  void call(Method Method, MethodModifier Modifier, const filt::Node* Nd) {
    Frame.ReturnValue = 0;
    FrameStack.push();
    Frame.CallMethod = Method;
    Frame.CallState = State::Enter;
    Frame.CallModifier = Modifier;
    Frame.Nd = Nd;
    Frame.ReturnValue = 0;
  }

  void popAndReturn(decode::IntType Value = 0) {
    FrameStack.pop();
    Frame.ReturnValue = Value;
    TRACE(IntType, "returns", Value);
  }

  void fail();
  void failBadState();
  void failNotImplemented();
  void failCantWrite();
  void failFreezingEof();

  // For debugging only.

  void traceEnterFrame() {
    assert(Frame.CallState == State::Enter);
    TRACE_BLOCK(traceEnterFrameInternal(););
  }
  void traceEnterFrameInternal();
  void traceExitFrame() { TRACE_EXIT_OVERRIDE(getName(Frame.CallMethod)); }

  void describeFrameStack(FILE* Out);
  void describeCallingEvalStack(FILE* Out);
  void describePeekPosStack(FILE* Out);
  void describeLoopCounterStack(FILE* Out);
  void describeLocalsStack(FILE* Out);
  void describeOpcodeLocalStack(FILE* Out);
  virtual void describeState(FILE* Out);
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_READER_H
