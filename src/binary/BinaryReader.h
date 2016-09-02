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

// Defines how to read filter sections in a WASM file.

#ifndef DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
#define DECOMPRESSOR_SRC_BINARY_BINARYREADER_H

#include "binary/BinaryReader.def"
#include "binary/SectionSymbolTable.h"
#include "interp/ReadStream.h"
#include "interp/TraceSexpReader.h"
#include "sexp/Ast.h"
#include "stream/Queue.h"
#include "stream/ReadCursor.h"
#include "stream/WriteCursor.h"
#include "utils/Defs.h"
#include "utils/ValueStack.h"

#include <functional>
#include <vector>

using namespace wasm::interp;

namespace wasm {

namespace filt {

class BinaryReader : public std::enable_shared_from_this<BinaryReader> {
  BinaryReader() = delete;
  BinaryReader(const BinaryReader&) = delete;
  BinaryReader& operator=(const BinaryReader&) = delete;

 public:
  // Internal state for resuming.
  enum class RunMethod {
#define X(tag, name) tag,
    BINARY_READER_METHODS_TABLE
#undef X
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    NO_SUCH_METHOD
  };
  static const char* getName(RunMethod Method);

  enum class RunState {
#define X(tag, name) tag,
    BINARY_READER_STATES_TABLE
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    NO_SUCH_STATE
#undef X
  };
  static const char* getName(RunState State);

  // Returns true if it begins with a WASM file magic number.
  static bool isBinary(const char* Filename);

  // Use resume() to continue.
  void startReadingSection();

  // Use resume() to continue.
  void startReadingFile();

  // Continues to read until finishedProcessingInput().
  void resume();

  // Returns the parsed file (or nullptr if unsuccessful).
  FileNode* getFile() {
    return (isSuccessful() && Frame.Method == RunMethod::Finished)
        ? CurFile : nullptr;
  }

  // Returns the parsed section (or nullptr if unsuccessful).
  SectionNode* getSection() {
    return (isSuccessful() && Frame.Method == RunMethod::Finished)
        ? CurSection : nullptr;
  }

  bool isFinished() const { return Frame.State == RunState::Succeeded; }

  bool isSuccessful() const {
    return Frame.State == RunState::Succeeded;
  }

  bool errorsFound() const {
    return Frame.State == RunState::Failed;
  }

  bool isEofFrozen() const { return ReadPos.isEofFrozen(); }

  BinaryReader(std::shared_ptr<decode::Queue> Input,
               std::shared_ptr<SymbolTable> Symtab);

  ~BinaryReader() {}

  // Returns the input buffer to the caller, so that they can
  // access it.
  const std::shared_ptr<decode::Queue> &getInput() const { return Input; }

  FileNode* readFile();

  SectionNode* readSection();

  TraceClassSexpReader& getTrace() { return Trace; }

  TextWriter* getTextWriter() const {
    return Trace.getTextWriter();
  }

 private:
  std::shared_ptr<interp::ReadStream> Reader;
  decode::ReadCursor ReadPos;
  decode::WriteCursor FillPos;
  std::shared_ptr<decode::Queue> Input;
  std::shared_ptr<SymbolTable> Symtab;
  SectionSymbolTable SectionSymtab;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  mutable TraceClassSexpReader Trace;

  template <typename T, typename... Args>
  T* create(Args&&... args) {
    return Symtab->create<T>(std::forward<Args>(args)...);
  }
  // Stop processing and fail.
  void fail();
  void fail(const std::string& Message);

  // True if resume can continue without needing more input.
  bool hasEnoughHeadroom() const;

  // Runs methods will read-fill of input.
  void readBackFilled();

  FileNode* readHeader();

  // General ast readers.
  template <class T>
  void readNullary();
  template <class T>
  void readUnary();
  template <class T>
  void readUnarySymbol();
  template <class T>
  void readUint8();
  template <class T>
  void readVarint32();
  template <class T>
  void readVarint64();
  template <class T>
  void readVaruint32();
  template <class T>
  void readVaruint64();
  template <class T>
  void readBinary();
  template <class T>
  void readBinarySymbol();
  template <class T>
  void readTernary();
  template <class T>
  void readNary();

  // Define state of nested methods.
  struct CallFrame {
    CallFrame() { init(); }
    CallFrame(RunMethod Method, RunState State)
        : Method(Method), State(State) {}
    void init() {
      // Optimistically, assume we succeed.
      Method = RunMethod::Started;
      State = RunState::Enter;
    }
    void fail() {
      Method = RunMethod::Finished;
      State = RunState::Failed;
    }
    RunMethod Method;
    RunState State;
    // For debugging
    void describe(FILE* Out) const;
  };

  ExternalName Name;
  FileNode *CurFile;
  SectionNode *CurSection;
  RunMethod CurBlockApplyFcn;
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  std::vector<Node*> NodeStack;
  size_t Counter;
  utils::ValueStack<size_t> CounterStack;

  // Schedules CallingMethod to be run next (i.e. the call will happen in the
  // next iteration of resume()).
  void call(RunMethod CallingMethod) {
    FrameStack.push();
    Frame.Method = CallingMethod;
    Frame.State = RunState::Enter;
    TRACE_ENTER(getName(CallingMethod));
  }

  // Schedule a return from the current method (i.e the return will happen in
  // the next iteration of resume()).
  void returnFromCall() {
    TRACE_EXIT_OVERRIDE(getName(Frame.Method));
    FrameStack.pop();
  }

  void describeFrameStack(FILE* Out) const;
  void describeCounterStack(FILE* Out) const;
  void describeCurBlockApplyFcn(FILE* Out) const;
  void describeNodeStack(FILE* Out, TextWriter* Writer) const;
  void describeRunState(FILE* Out) const;
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
