/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Defines how to read filter sections in a WASM file.

#ifndef DECOMPRESSOR_SRC_BINARY_BINARYREADER_H
#define DECOMPRESSOR_SRC_BINARY_BINARYREADER_H

#include "binary/SectionSymbolTable.h"
#include "interp/ReadStream.h"
#include "interp/TraceSexpReader.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "stream/Queue.h"
#include "stream/ReadCursor.h"
#include "stream/WriteCursor.h"
#include "utils/Defs.h"

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
  enum class RunMethod { File,  Section, Unknown };
  enum class RunState { Enter, Exit, SectionNodeLoop, FileSectionLoop, Fail, Success };
  class Runner {
    friend class BinaryReader;
   public:
    Runner(std::shared_ptr<BinaryReader> Reader,
           std::shared_ptr<decode::ReadCursor> ReadPos)
        : Reader(Reader), ReadPos(ReadPos),
          CurMethod(RunMethod::Unknown),
          CurState(RunState::Fail),
          CurFile(nullptr),
          CurSection(nullptr) {
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
      return CurState == RunState::Success;
    }
    bool errorsFound() const {
      return CurState == RunState::Fail;
    }
    bool isReadingSection() const {
      return CurMethod == RunMethod::Section;
    }
    SectionNode* getSection() {
      return (isSuccessful() && isReadingSection())  ? CurSection : nullptr;
    }
    bool isReadingFile() const {
      return CurMethod == RunMethod::File;
    }
    FileNode* getFile() {
      return (isSuccessful() && isReadingFile()) ? CurFile : nullptr;
    }
    bool isEofFrozen() const { return ReadPos->isEofFrozen(); }
    size_t filledSize() const { return FillPos->getCurByteAddress(); }
    void resumeReading();

    void setTraceProgress(bool NewValue) {
      Reader->setTraceProgress(NewValue);
    }
    TraceClassSexpReader& getTrace() { return Reader->getTrace(); }
   private:
    struct CallFrame {
      CallFrame(RunMethod Method, RunState State)
          : Method(Method), State(State) {}
      RunMethod Method;
      RunState State;
    };
    std::shared_ptr<BinaryReader> Reader;
    std::shared_ptr<decode::ReadCursor> ReadPos;
    std::shared_ptr<decode::WriteCursor> FillPos;
    std::vector<CallFrame> CallStack;
    RunMethod CurMethod;
    RunState CurState;
    FileNode *CurFile;
    SectionNode *CurSection;
    bool hasEnoughHeadroom() const;
    void pushFrame(RunMethod NewMethod) {
      CallStack.push_back(CallFrame(CurMethod, CurState));
      CurMethod = NewMethod;
      CurState = RunState::Enter;
    }
    void popFrame() {
      CallFrame &Frame = CallStack.back();
      CurMethod = Frame.Method;
      CurState = Frame.State;
      CallStack.pop_back();
    }
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

  class UsingReadPos {
   public:
    UsingReadPos(BinaryReader &Reader, decode::ReadCursor &ReadPos)
        : Reader(Reader), OldReadPos(&ReadPos) {
      Reader.ReadPos = &ReadPos;
      if (isDebug())
        Reader.Trace.setReadPos(&ReadPos);
    }
    ~UsingReadPos() {
      Reader.ReadPos = OldReadPos;
      if (isDebug())
        Reader.Trace.setReadPos(OldReadPos);
    }
   private:
    BinaryReader& Reader;
    decode::ReadCursor *OldReadPos;
  };

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
