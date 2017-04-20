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

// Defines an interpreter of that reads a file and applies algorithms to the
// data in the file.

#ifndef DECOMPRESSOR_SRC_INTERP_INTERPRETER_H_
#define DECOMPRESSOR_SRC_INTERP_INTERPRETER_H_

#include "interp/Interpreter.def"
#include "interp/InterpreterFlags.h"
#include "stream/ValueFormat.h"
#include "utils/TraceAPI.h"
#include "utils/ValueStack.h"

namespace wasm {

namespace filt {

class Case;
class Eval;
class Header;
class Node;
class SymbolTable;
class TextWriter;

}  // end of namespace filt.

namespace interp {

class AlgorithmSelector;
class Interpreter;
class Reader;
class Writer;

class Interpreter {
  Interpreter() = delete;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

 public:
  enum class Method {
#define X(tag) tag,
    INTERPRETER_METHODS_TABLE
#undef X
        NO_SUCH_METHOD
  };
  static const char* getName(Method M);

  Interpreter(std::shared_ptr<Reader> Input,
              std::shared_ptr<Writer> Output,
              const InterpreterFlags& Flags,
              std::shared_ptr<filt::SymbolTable> Symtab);
  Interpreter(std::shared_ptr<Reader> Input,
              std::shared_ptr<Writer> Output,
              const InterpreterFlags& Flags);
  virtual ~Interpreter();

  void useFileHeader(const filt::Header* Hdr) { HeaderOverride = Hdr; }

  const InterpreterFlags& getFlags() const { return Flags; }

  std::shared_ptr<Reader> getInput() { return Input; }
  void setInput(std::shared_ptr<Reader> Value);

  std::shared_ptr<Writer> getWriter() { return Output; }
  void setWriter(std::shared_ptr<Writer> Value);

  std::shared_ptr<filt::SymbolTable> getSymbolTable() { return Symtab; }
  void setSymbolTable(std::shared_ptr<filt::SymbolTable> NewSymtab) {
    Symtab = NewSymtab;
  }
  void resetSymbolTable() { Symtab.reset(); }

  bool getFreezeEofAtExit() { return FreezeEofAtExit; }
  void setFreezeEofAtExit(bool NewValue) { FreezeEofAtExit = NewValue; }

  void addSelector(std::shared_ptr<AlgorithmSelector> Selector);

  std::shared_ptr<filt::SymbolTable> getDefaultAlgorithm(
      const filt::Node* TargetHeader);

  // Starts up decompression using an algorithm.
  void algorithmStart();
  void algorithmStartHasFileHeader();

  // Resumes decompression where it left off. Assumes that more
  // input has been added since the previous start()/resume() call.
  // Resume should be called until isFinished() is true.
  void algorithmResume();

  // Reads from backfilled input stream.
  void algorithmReadBackFilled();

  void algorithmRead();

  // Check status of read.
  bool isFinished() const { return Frame.CallMethod == Method::Finished; }
  bool isSuccessful() const { return Frame.CallState == State::Succeeded; }
  bool errorsFound() const { return Frame.CallState == State::Failed; }
  bool hasReadMode() const { return isReadModifier(Frame.CallModifier); }
  bool hasWriteMode() const { return isWriteModifier(Frame.CallModifier); }

  // Throw exception (with message) for error.
  void throwMessage(const std::string& Message);
  void throwMessage(const std::string& Message, decode::IntType Value);
  void throwCantRead();
  void throwCantWrite();
  void throwCantFreezeEof();
  void throwCantWriteInWriteOnlyMode();
  void throwBadHeaderValue(decode::IntType WantedValue,
                           decode::IntType FoundValue,
                           decode::ValueFormat Format);

  // After catching (and cleaning up state), rethrow caught exception error.
  void rethrow();

  bool canCatchThrow();

  // Finds state to apply catch, or fail if no catch is defined.
  void catchOrElseFail();

  // Fail always (even if catch applicable).
  void fail(const std::string& Message);
  void failBadState();
  void failNotImplemented();

  // Returns non-null context handler if applicable.
  utils::TraceContextPtr getTraceContext();
  void setTraceProgress(bool NewValue);
  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }

  enum class SectionCode : uint32_t {
#define X(code, value) code = value,
    SECTION_CODES_TABLE
#undef X
        NO_SUCH_SECTION_CODE
  };

  static const char* getName(SectionCode Code);

 protected:
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
    CallFrame();
    CallFrame(Method CallMethod, const filt::Node* Nd);
    CallFrame(const CallFrame& M);
    void reset();
    void fail();
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
    EvalFrame();
    EvalFrame(const filt::Eval* Caller, size_t CallingEvalIndex);
    EvalFrame(const EvalFrame& F);
    bool isDefined() const;
    void reset();
    void describe(FILE* File, filt::TextWriter* Writer) const;
    const filt::Eval* Caller;
    size_t CallingEvalIndex;
  };

  std::shared_ptr<Reader> Input;
  std::shared_ptr<Writer> Output;
  const InterpreterFlags& Flags;
  std::shared_ptr<filt::SymbolTable> Symtab;
  std::vector<std::shared_ptr<AlgorithmSelector>> Selectors;
  // The current section name (if applicable).
  std::string CurSectionName;
  // The last read value.
  decode::IntType LastReadValue;
  // Holds the method to call (i.e. dispatch) if code expects a method to be
  // provided by the caller.
  Method DispatchedMethod;
  // The last thrown message. Used for rethrowing.
  std::string RethrowMessage;

  // Trace object to use, if applicable.
  std::shared_ptr<utils::TraceClass> Trace;

  // Defines method to fail back to (defaults to
  // NO_SUCH_METHOD). Allows equivalent of simple throws.
  Method Catch;
  utils::ValueStack<Method> CatchStack;
  // Holds the state before modified to state.
  State CatchState;
  // Fatal override that causes reader to fail, even wth the presence of
  // catches.
  bool IsFatalFailure;
  bool CheckForEof;
  // The stack of called methods.
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  // The stack of (eval) calls.
  EvalFrame CallingEval;
  utils::ValueStack<EvalFrame> CallingEvalStack;
  // The stack of loop counters.
  size_t LoopCounter;
  utils::ValueStack<size_t> LoopCounterStack;
  // The stack of local values. Also includes expression arguments
  // (appearing after user-defined locals).
  size_t LocalsBase;
  utils::ValueStack<size_t> LocalsBaseStack;
  std::vector<decode::IntType> LocalValues;

  // The stack of opcode Selshift/CaseMasks for multi-byte opcodes.
  struct OpcodeLocalsFrame {
    uint32_t SelShift;
    decode::IntType CMask;
    const filt::Case* C;
    OpcodeLocalsFrame() { reset(); }
    void describe(FILE* File, filt::TextWriter* Writer = nullptr) const;
    void reset() {
      SelShift = 0;
      CMask = 0;
      C = nullptr;
    }
  };
  OpcodeLocalsFrame OpcodeLocals;
  utils::ValueStack<OpcodeLocalsFrame> OpcodeLocalsStack;

  const filt::Header* HeaderOverride;
  bool FreezeEofAtExit;

  void reset();
  void algorithmStart(Method M) { callTopLevel(M, nullptr); }
  void handleOtherMethods();

  // Initializes all internal stacks, for an initial call to Method with
  // argument Nd.
  void callTopLevel(Method Method, const filt::Node* Nd);

  void call(Method Method, MethodModifier Modifier, const filt::Node* Nd);

  void popAndReturn(decode::IntType Value = 0);

  // For debugging only.
  void traceEnterFrame();
  void traceEnterFrameInternal();
  void traceExitFrame();

  void describeFrameStack(FILE* Out);
  void describeCallingEvalStack(FILE* Out);
  void describePeekPosStack(FILE* Out);
  void describeLoopCounterStack(FILE* Out);
  void describeLocalsStack(FILE* Out);
  void describeOpcodeLocalsStack(FILE* Out);
  virtual void describeState(FILE* Out);
  virtual const char* getDefaultTraceName() const;

  void init();
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H_
