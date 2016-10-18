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
#include "interp/TraceSexpReaderWriter.h"
#include "interp/WriteStream.h"
#include "stream/WriteCursor.h"

namespace wasm {

namespace filt {
class TextWriter;
}

namespace interp {

// Defines the algorithm interpreter for filter sections, and the
// corresponding state associated with the interpreter.
//
// TODO(karlschimpf) Rename this to a better name.
class Interpreter FINAL : public Reader {
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

  // Resumes decompression where it left off. Assumes that more
  // input has been added since the previous start()/resume() call.
  // Resume should be called until isFinished() is true.
  void resume();

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  void setMinimizeBlockSize(bool NewValue) { MinimizeBlockSize = NewValue; }

  TraceClassSexpReaderWriter& getTrace() { return Trace; }

 private:
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

  void clearStacksExceptFrame() OVERRIDE;
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

  // Sets up code to call write method Method with arguments Nd and WriteValue.
  // Note: Method may not be Method::Write. Rather, it may be some intermediate
  // method that sets up a call to Method::Write using field DispatchesMethod.
  void callWrite(Method Method,
                 const filt::Node* Nd,
                 decode::IntType WriteValue) {
    call(Method, MethodModifier::ReadAndWrite, Nd);
    WriteValueStack.push(WriteValue);
  }

  void readBackFilled();

  // For debugging only.
  void describeWriteValueStack(FILE* Out);
  void describeBlockStartStack(FILE* Out);
  void describeAllNonemptyStacks(FILE* Out) OVERRIDE;
};

}  // end of namespace interp.

}  // end of namespace wasm.

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_H
