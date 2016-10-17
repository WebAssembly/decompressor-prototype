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

#include "interp/Reader.h"
#include "stream/WriteCursor.h"
#include "interp/WriteStream.h"

namespace wasm {

namespace filt {
class TextWriter;
}

namespace interp {

// Defines the algorithm interpreter for filter sections, and the
// corresponding state associated with the interpreter.
//
// TODO(karlschimpf) Rename this to a better name.
class Interpreter : public Reader {
  Interpreter() = delete;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

 public:
  Interpreter(std::shared_ptr<decode::Queue> Input,
              std::shared_ptr<decode::Queue> Output,
              std::shared_ptr<filt::SymbolTable> Symtab);

  ~Interpreter() {}

  // Processes each section in input, and decompresses it (if applicable)
  // to the corresponding output.
  void decompress();

  // Starts up decompression.
  void start() {
    assert(FrameStack.empty());
    callTopLevel(Method::GetFile, nullptr);
  }

  // Resumes decompression where it left off. Assumes that more
  // input has been added since the previous start()/resume() call.
  // Resume should be called until isFinished() is true.
  void resume();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

  bool isFinished() const { return Frame.CallMethod == Method::Finished; }
  bool isSuccessful() const { return Frame.CallState == State::Succeeded; }
  bool errorsFound() const { return Frame.CallState == State::Failed; }

  // Force interpretation to fail.
  void fail(const std::string& Message);

  TraceClassSexpReaderWriter& getTrace() { return Trace; }

 private:
  void fail();

  std::shared_ptr<filt::SymbolTable> Symtab;
  decode::WriteCursor WritePos;
  std::shared_ptr<WriteStream> Writer;
  filt::Node* DefaultFormat;
  bool MinimizeBlockSize;
  TraceClassSexpReaderWriter Trace;

  // The stack of passed write Values.
  decode::IntType WriteValue;
  utils::ValueStack<decode::IntType> WriteValueStack;
  // The stack of block patch locations.
  decode::WriteCursor BlockStart;
  utils::ValueStack<decode::WriteCursor> BlockStartStack;

  void failBadState();
  void failNotImplemented();
  void failCantWrite();

  // Initializes all internal stacks, for an initial call to Method with
  // argument Nd.
  void callTopLevel(Method Method, const filt::Node* Nd) OVERRIDE;

  void call(Method Method, MethodModifier Modifier, const filt::Node* Nd) {
    Frame.ReturnValue = 0;
    FrameStack.push();
    Frame.CallMethod = Method;
    Frame.CallState = State::Enter;
    Frame.CallModifier = Modifier;
    Frame.Nd = Nd;
    Frame.ReturnValue = 0;
  }

  bool hasReadMode() const {
    return isReadModifier(Frame.CallModifier);
  }
  bool hasWriteMode() const {
    return isWriteModifier(Frame.CallModifier);
  }

  // Sets up code to call write method Method with arguments Nd and WriteValue.
  // Note: Method may not be Method::Write. Rather, it may be some intermediate
  // method that sets up a call to Method::Write using field DispatchesMethod.
  void callWrite(Method Method,
                 const filt::Node* Nd,
                 decode::IntType WriteValue) {
    call(Method, MethodModifier::ReadAndWrite, Nd);
    WriteValueStack.push(WriteValue);
  }

  void popAndReturn(decode::IntType Value = 0) {
    FrameStack.pop();
    Frame.ReturnValue = Value;
    TRACE(IntType, "returns", Value);
  }

  void traceEnterFrame() {
    assert(Frame.CallState == Interpreter::State::Enter);
    TRACE_BLOCK(traceEnterFrameInternal(););
  }
  void traceEnterFrameInternal();
  void traceExitFrame() { TRACE_EXIT_OVERRIDE(getName(Frame.CallMethod)); }

  void readBackFilled();

  // For debugging only.
  void describeWriteValueStack(FILE* Out);
  void describeBlockStartStack(FILE* Out);
  void describeAllNonemptyStacks(FILE* Out) OVERRIDE;
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
