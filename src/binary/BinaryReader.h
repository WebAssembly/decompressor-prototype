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

  // Locks ReadPos to reader, for scope of this.
  class UsingReadPos {
   public:
    UsingReadPos(BinaryReader &Reader, decode::ReadCursor &ReadPos)
        : Reader(Reader), OldReadPos(&ReadPos) {
      Reader.ReadPos = &ReadPos;
      if (isDebug())
        Reader.Trace.setReadPos(&ReadPos);
    }
    UsingReadPos(std::shared_ptr<BinaryReader> Reader,
                 std::shared_ptr<decode::ReadCursor> ReadPos)
        : Reader(*Reader.get()), OldReadPos(ReadPos.get()) {}
    ~UsingReadPos() {
      Reader.ReadPos = OldReadPos;
      if (isDebug())
        Reader.Trace.setReadPos(OldReadPos);
    }
   private:
    BinaryReader& Reader;
    decode::ReadCursor *OldReadPos;
  };

  // Internal state for resuming.
  // TODO(karlschimpf) Create ".def" file to define names for these.
  enum class RunMethod {
#define X(tag, name) tag,
    BINARY_READER_METHODS_TABLE
#undef X
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    RunMethod_NO_SUCH_METHOD
  };
  enum class RunState {
#define X(tag, name) tag,
    BINARY_READER_STATES_TABLE
    // The following is necessary for some compilers, because otherwise a comma
    // warning may occur.
    RunState_NO_SUCH_STATE
#undef X
  };
  class Runner {
    friend class BinaryReader;
   public:
    Runner(std::shared_ptr<BinaryReader> Reader,
           std::shared_ptr<decode::ReadCursor> ReadPos)
        : Reader(Reader), ReadPos(ReadPos),
          CurFile(nullptr),
          CurSection(nullptr),
          CurBlockApplyFcn(RunMethod::RunMethod_NO_SUCH_METHOD),
          FrameStack(Frame),
          CounterStack(Counter) {
      FillPos = std::make_shared<decode::WriteCursor>(
          *ReadPos.get(), ReadPos->getCurByteAddress());
    }
    std::shared_ptr<decode::ReadCursor> getReadPos() const {
      return ReadPos;
    }
    std::shared_ptr<decode::WriteCursor> getFillPos() const {
      return FillPos;
    }
    bool needsMoreInput() const {
      return !isSuccessful() && !errorsFound();
    }
    bool isSuccessful() const {
      return Frame.State == RunState::Succeeded;
    }
    bool errorsFound() const {
      return Frame.State == RunState::Failed;
    }
    SectionNode* getSection() {
      return (isSuccessful() && Frame.Method == RunMethod::Finished)
          ? CurSection : nullptr;
    }
    FileNode* getFile() {
      return (isSuccessful() && Frame.Method == RunMethod::Finished)
          ? CurFile : nullptr;
    }
    bool isEofFrozen() const { return FillPos->isEofFrozen(); }
    size_t filledSize() const { return FillPos->getCurByteAddress(); }
    void resumeReading();
    template <typename T, typename... Args>
    T* create(Args&&... args) {
      return Reader->Symtab->create<T>(std::forward<Args>(args)...);
    }
    void setTraceProgress(bool NewValue) {
      Reader->setTraceProgress(NewValue);
    }
    TraceClassSexpReader& getTrace() { return Reader->getTrace(); }
   private:

    // Define state of nested methods.
    struct CallFrame {
      CallFrame() { init(); }
      CallFrame(RunMethod Method, RunState State)
          : Method(Method), State(State) {}
      void init() {
        // Optimistically, assume we succeed.
        Method = RunMethod::Finished;
        State = RunState::Succeeded;
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

    std::shared_ptr<BinaryReader> Reader;
    std::shared_ptr<decode::ReadCursor> ReadPos;
    std::shared_ptr<decode::WriteCursor> FillPos;
    ExternalName Name;
    FileNode *CurFile;
    SectionNode *CurSection;
    RunMethod CurBlockApplyFcn;
    CallFrame Frame;
    utils::ValueStack<CallFrame> FrameStack;
    size_t Counter;
    utils::ValueStack<size_t> CounterStack;

    bool hasEnoughHeadroom() const;

    // Schedules CallingMethod to be run next.
    void call(RunMethod CallingMethod) {
      FrameStack.push();
      Frame.Method = CallingMethod;
      Frame.State = RunState::Enter;
    }

    // Stop processing and fail.
    void fail();

    void describeFrames(FILE* Out);

  };

  // Returns true if it begins with a WASM file magic number.
  static bool isBinary(const char* Filename);

  static std::shared_ptr<Runner>
  startReadingSection(std::shared_ptr<decode::ReadCursor> ReadPos,
                      std::shared_ptr<SymbolTable> Symtab);

  static std::shared_ptr<Runner>
  startReadingFile(std::shared_ptr<decode::ReadCursor> ReadPos,
                   std::shared_ptr<SymbolTable> Symtab);

  BinaryReader(std::shared_ptr<decode::Queue> Input,
               std::shared_ptr<SymbolTable> Symtab);

  ~BinaryReader() {}

  // Returns the input buffer to the caller, so that they can
  // access it.
  const std::shared_ptr<decode::Queue> &getInput() const { return Input; }

  FileNode* readFile(decode::StreamType = decode::StreamType::Byte);

  FileNode* readFile(decode::ReadCursor &ReadPos);

  SectionNode* readSection(decode::StreamType = decode::StreamType::Byte);

  SectionNode* readSection(decode::ReadCursor &ReadPos);

  void setTraceProgress(bool NewValue) { Trace.setTraceProgress(NewValue); }

  TraceClassSexpReader& getTrace() { return Trace; }

 private:
  std::shared_ptr<interp::ReadStream> Reader;
  std::shared_ptr<decode::Queue> Input;
  decode::ReadCursor *ReadPos;
  std::shared_ptr<SymbolTable> Symtab;
  SectionSymbolTable SectionSymtab;
  // The magic number of the input.
  uint32_t MagicNumber;
  // The version of the input.
  uint32_t Version;
  std::vector<Node*> NodeStack;
  TraceClassSexpReader Trace;

  // Reads in a name and returns the read name. Reference is only good till
  // next call to readInternalName() or ReadExternalName().
  InternalName& readInternalName();
  ExternalName& readExternalName();

  FileNode* readHeader();
  void readBlock(std::function<void()> ApplyFn);
  void readSymbolTable();
  void readNode();

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
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
