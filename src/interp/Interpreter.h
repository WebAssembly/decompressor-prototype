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
  enum class InterpMethod {
#define X(tag, name) tag,
    INTERPRETER_METHODS_TABLE
#undef X
        NO_SUCH_METHOD
  };

  enum class InterpState {
#define X(tag, name) tag,
    INTERPRETER_STATES_TABLE
#undef X
        NO_SUCH_STATE
  };

  // The call stack of methods being applied.
  struct CallFrame {
    CallFrame() { reset(); }
    CallFrame(InterpMethod Method, const filt::Node* Nd)
        : Method(Method), State(InterpState::Enter), Nd(Nd) {}
    CallFrame(const CallFrame& M)
        : Method(M.Method), State(M.State), Nd(M.Nd) {}
    void reset() {
      Method = InterpMethod::Finished;
      State = InterpState::Succeeded;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void fail() {
      Method = InterpMethod::Finished;
      State = InterpState::Failed;
      Nd = nullptr;
      ReturnValue = 0;
    }
    void describe(FILE* File, filt::TextWriter* Writer) const;
    InterpMethod Method;
    InterpState State;
    const filt::Node* Nd;
    // Holds return value from last called routine, except when
    // exiting.  Note: For method write, this corresponds to the value
    // to write as well.
    decode::IntType ReturnValue;
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
  bool MinimizeBlockSize;
  TraceClassSexpReaderWriter Trace;
  // The stack of (eval) calls.
  filt::ConstNodeVectorType EvalStack;

  // Tracks the call stack of methods.
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  // The stack of passed Values (write methods).
  decode::IntType WriteValue;
  utils::ValueStack<decode::IntType> WriteValueStack;
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
      Case = 0;
    }
  };
  OpcodeLocalsFrame OpcodeLocals;
  utils::ValueStack<OpcodeLocalsFrame> OpcodeLocalsStack;

  void fail();

  void callTopLevel(InterpMethod Method,
                    const filt::Node* Nd,
                    decode::IntType ReturnValue = 0);

  void call(InterpMethod Method,
            const filt::Node* Nd,
            decode::IntType ReturnValue = 0) {
    FrameStack.push();
    Frame.Method = Method;
    Frame.State = InterpState::Enter;
    Frame.Nd = Nd;
    Frame.ReturnValue = ReturnValue;
  }

  void describeFrameStack(FILE* Out);

  TraceClassSexpReaderWriter& getTrace() { return Trace; }
  void decompressSection();
  void readSectionName();
  void decompressBlock(const filt::Node* Code);
  // Evaluates code if nonnull. Otherwise copies to end of block.
  void evalOrCopy(const filt::Node* Code);

  // Evaluates Nd. Returns read value if applicable. Zero otherwise.
  decode::IntType eval(const filt::Node* Nd);
  // Reads input as defined by Nd. Returns read value.
  decode::IntType read(const filt::Node* Nd);
  // Writes to output the given value, using format defined by Nd.
  // For convenience, returns written value.
  decode::IntType write(decode::IntType Value, const filt::Node* Nd);
  // Internal driver for eval/read/write.
  decode::IntType readOpcode(const filt::Node* Sel,
                             decode::IntType PrefixValue,
                             uint32_t NumOpcodes);
  // Reads opcode selector into Value. Returns the Bitsize to the (fixed) number
  // of bits used to read the opcode selector. Otherwise returns zero.
  uint32_t readOpcodeSelector(const filt::Node* Nd, decode::IntType& Value);
  const filt::Node* getParam(const filt::Node* Param);

  // Stack model
  void runMethods();
  void popAndReturn(decode::IntType Value) {
    FrameStack.pop();
    Frame.ReturnValue = Value;
    TRACE(IntType, "returns", Value);
  }
  void installWriteValue() {
    WriteValueStack.push();
    WriteValue = Frame.ReturnValue;
    Frame.ReturnValue = 0;
    TRACE(IntType, "WriteValue", WriteValue);
  }
  void popAndReturnWriteValue() {
    popAndReturn(WriteValue);
    WriteValueStack.pop();
  }
  void popAndReturnReadValue(decode::IntType Value) {
    LastReadValue = Value;
    popAndReturn(Value);
  }

  bool hasEnoughHeadroom() const;
  bool needsMoreInput() const { return !isSuccessful() && !errorsFound(); }
  bool isFinished() const { return Frame.Method == InterpMethod::Finished; }
  bool isSuccessful() const { return Frame.State == InterpState::Succeeded; }
  bool errorsFound() const { return Frame.State == InterpState::Failed; }

  // Fills input stream using the read backing of the input stream, so that
  // runMethods can use a push model.
  void readBackFilled();
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
