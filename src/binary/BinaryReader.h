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
#include "interp/ByteReadStream.h"
#include "interp/ReadStream.h"
#include "sexp/Ast.h"
#include "sexp/TraceSexp.h"
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
  enum class Method {
#define X(tag, name) tag,
    BINARY_READER_METHODS_TABLE
#undef X
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    NO_SUCH_METHOD
  };
  static const char* getName(Method CallMethod);

  enum class State {
#define X(tag, name) tag,
    BINARY_READER_STATES_TABLE
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    NO_SUCH_STATE
#undef X
  };
  static const char* getName(State CallState);

  // Returns true if it begins with a WASM file magic number.
  static bool isBinary(const char* Filename);

  // Use resume() to continue.
  void startReadingFile();

  // Continues to read until finishedProcessingInput().
  void resume();

  // Returns the parsed file (or nullptr if unsuccessful).
  FileNode* getFile() { return isSuccessful() ? CurFile : nullptr; }

  // Returns the parsed section (or nullptr if unsuccessful).
  SectionNode* getSection() { return isSuccessful() ? CurSection : nullptr; }

  bool isFinished() const { return Frame.CallMethod == Method::Finished; }

  bool isSuccessful() const { return Frame.CallState == State::Succeeded; }

  bool errorsFound() const { return Frame.CallState == State::Failed; }

  bool isEofFrozen() const { return ReadPos.isEofFrozen(); }

  BinaryReader(std::shared_ptr<decode::Queue> Input,
               std::shared_ptr<SymbolTable> Symtab);

  ~BinaryReader() {}

  // Returns the input buffer to the caller, so that they can
  // access it.
  const std::shared_ptr<decode::Queue>& getInput() const { return Input; }

  FileNode* readFile();

  void setTraceProgress(bool NewValue) {
    getTrace().setTraceProgress(NewValue);
  }
  void setTrace(std::shared_ptr<TraceClassSexp> Trace);
  TraceClassSexp& getTrace() const;

 private:
  struct CallFrame {
    CallFrame() { init(); }
    CallFrame(Method CallMethod, State CallState)
        : CallMethod(CallMethod), CallState(CallState) {}
    void init() {
      // Optimistically, assume we succeed.
      CallMethod = Method::Started;
      CallState = State::Enter;
    }
    void fail() {
      CallMethod = Method::Finished;
      CallState = State::Failed;
    }
    Method CallMethod;
    State CallState;
    // For debugging
    void describe(FILE* Out) const;
  };

  std::shared_ptr<interp::ByteReadStream> Reader;
  decode::ReadCursorWithTraceContext ReadPos;
  decode::WriteCursor FillPos;
  std::shared_ptr<decode::Queue> Input;
  std::shared_ptr<SymbolTable> Symtab;
  SectionSymbolTable SectionSymtab;
#if 0
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t CasmVersion;
  uint32_t WasmVersion;
#endif
  mutable std::shared_ptr<TraceClassSexp> Trace;
  std::string Name;
  FileNode* CurFile;
  SectionNode* CurSection;
  Method CurBlockApplyFcn;
  CallFrame Frame;
  utils::ValueStack<CallFrame> FrameStack;
  std::vector<Node*> NodeStack;
  size_t Counter;
  utils::ValueStack<size_t> CounterStack;

  template <typename T, typename... Args>
  T* create(Args&&... args) {
    return Symtab->create<T>(std::forward<Args>(args)...);
  }

  // Schedules CallMethod to be run next (i.e. the call will happen in the
  // next iteration of resume()).
  void call(Method CallMethod) {
    FrameStack.push();
    Frame.CallMethod = CallMethod;
    Frame.CallState = State::Enter;
    TRACE_ENTER(getName(CallMethod));
  }

  // Schedule a return from the current method (i.e the return will happen in
  // the next iteration of resume()).
  void returnFromCall() {
    TRACE_EXIT_OVERRIDE(getName(Frame.CallMethod));
    FrameStack.pop();
  }

  // Stop processing and fail.
  void fail();
  void fail(const std::string& Message);
  void failBadState();

  // True if resume can continue without needing more input.
  bool hasEnoughHeadroom() const;

  // Runs methods will read-fill of input.
  void readBackFilled();

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

  void describeFrameStack(FILE* Out) const;
  void describeCounterStack(FILE* Out) const;
  void describeCurBlockApplyFcn(FILE* Out) const;
  void describeNodeStack(FILE* Out) const;
  void describeState(FILE* Out) const;
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
