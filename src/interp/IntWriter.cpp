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

// Implements a writer for a (non-file based) integer stream.

#include "interp/IntWriter.h"

#include <unordered_set>
#include <vector>

#include "sexp/Ast.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

// This class is used to implement the Table operator interface. It uses a
// scratchpad for writing. This is done to simplify the write API. The
// cursors/methods do not need to know if they are working on the scratchpad
// instead of the output stream. This solution is inefficient. However,the
// assumption is that if you are writing to such a stream, you probably are
// running inside a compressor where the performance is not important.
class IntWriter::TableHandler {
  TableHandler() = delete;
  TableHandler(const TableHandler&) = delete;
  TableHandler& operator=(const TableHandler&) = delete;

 public:
  explicit TableHandler(IntWriter& Writer);
  ~TableHandler();
  bool tablePush(decode::IntType Value);
  bool tablePop();

 private:
  IntWriter& Writer;
  std::vector<IntStream::Ptr> FreeStreams;
  std::unordered_set<IntType> Cached;
  std::vector<bool> RestoreStack;
  std::vector<IntStream::WriteCursor> CursorStack;
  IntStream::Ptr allocStream();
  void freeStream(IntStream::Ptr Strm);
  static constexpr size_t MaxFreeStreams = 5;
};

IntWriter::TableHandler::TableHandler(IntWriter& Writer)
    : Writer(Writer), FreeStreams(MaxFreeStreams) {}

IntWriter::TableHandler::~TableHandler() {}

IntStream::Ptr IntWriter::TableHandler::allocStream() {
  if (FreeStreams.empty()) {
    IntStream::Ptr Strm = std::make_shared<IntStream>();
    Strm->closeHeader();
    return Strm;
  }
  IntStream::Ptr Ptr = FreeStreams.back();
  FreeStreams.pop_back();
  return Ptr;
}

void IntWriter::TableHandler::freeStream(IntStream::Ptr Strm) {
  if (FreeStreams.size() >= MaxFreeStreams)
    // Let the destructor of Strm return the memory.
    return;
  FreeStreams.push_back(Strm);
}

bool IntWriter::TableHandler::tablePush(decode::IntType Value) {
  if (Cached.count(Value)) {
    IntStream::WriteCursor TmpCursor(allocStream());
    CursorStack.push_back(Writer.Pos);
    Writer.Pos = TmpCursor;
    RestoreStack.push_back(true);
    return true;
  }
  Cached.insert(Value);
  RestoreStack.push_back(false);
  return true;
}

bool IntWriter::TableHandler::tablePop() {
  if (RestoreStack.empty())
    return false;
  bool Restore = RestoreStack.back();
  RestoreStack.pop_back();
  if (!Restore)
    return true;
  if (CursorStack.empty())
    return false;
  freeStream(Writer.Pos.getStream());
  Writer.Pos = CursorStack.back();
  CursorStack.pop_back();
  return true;
}

IntWriter::IntWriter(std::shared_ptr<IntStream> Output)
    : Writer(true), Output(Output), Pos(Output), TblHandler(nullptr) {}

IntWriter::~IntWriter() {
  delete TblHandler;
}

void IntWriter::reset() {
  Output->reset();
  IntStream::WriteCursor StartPos(Output);
  Pos = StartPos;
}

TraceContextPtr IntWriter::getTraceContext() {
  return Pos.getTraceContext();
}

const char* IntWriter::getDefaultTraceName() const {
  return "IntWriter";
}

StreamType IntWriter::getStreamType() const {
  return StreamType::Int;
}

bool IntWriter::writeVaruint64(uint64_t Value) {
  return write(Value);
}

bool IntWriter::writeBlockEnter() {
  return Pos.openBlock();
}

bool IntWriter::writeBlockExit() {
  return Pos.closeBlock();
}

bool IntWriter::writeFreezeEof() {
  return Pos.freezeEof();
}

bool IntWriter::writeHeaderValue(IntType Value, IntTypeFormat Format) {
#if 0
  fprintf(stderr, "Appending %" PRIuMAX "\n", uintmax_t(Value));
  Output->describe(stderr);
#endif
  Output->appendHeader(Value, Format);
  return true;
}

bool IntWriter::writeHeaderClose() {
  Output->closeHeader();
  return true;
}

bool IntWriter::tablePush(decode::IntType Value) {
  if (TblHandler == nullptr)
    TblHandler = new TableHandler(*this);
  return TblHandler->tablePush(Value);
}

bool IntWriter::tablePop() {
  if (TblHandler == nullptr)
    return false;
  return TblHandler->tablePop();
}

void IntWriter::describeState(FILE* File) {
  fprintf(stderr, "Pos = ");
  Pos.describe(File);
  fputc('\n', File);
}

}  // end of namespace interp

}  // end of namespace wasm
