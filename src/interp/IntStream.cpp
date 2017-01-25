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

// Implements a (non-file based) integer stream.

#include "interp/IntStream.h"

namespace wasm {

using namespace decode;
using namespace utils;

namespace interp {

void IntStream::Block::describe(FILE* File) {
  fprintf(File, "[%" PRIxMAX "", uintmax_t(BeginIndex));
  if (EndIndex != std::numeric_limits<size_t>::max())
    fprintf(File, ":%" PRIxMAX "", uintmax_t(EndIndex));
  fputc(']', File);
}

IntStream::Cursor::TraceContext::~TraceContext() {
}

void IntStream::Cursor::TraceContext::describe(FILE* File) {
  Pos.describe(File);
}

IntStream::Cursor::Cursor(StreamPtr Stream) : Index(0), Stream(Stream) {
  assert(Stream);
  EnclosingBlocks.push_back(Stream->TopBlock);
}

IntStream::Cursor::Cursor(const IntStream::Cursor& C)
    : std::enable_shared_from_this<Cursor>(C),
      Index(C.Index),
      EnclosingBlocks(C.EnclosingBlocks),
      Stream(C.Stream) {
}

IntStream::Cursor& IntStream::Cursor::operator=(const IntStream::Cursor& C) {
  Index = C.Index;
  EnclosingBlocks = C.EnclosingBlocks;
  Stream = C.Stream;
  return *this;
}

TraceClass::ContextPtr IntStream::Cursor::getTraceContext() {
  return std::make_shared<IntStream::Cursor::TraceContext>(*this);
}

FILE* IntStream::Cursor::describe(FILE* File,
                                  bool IncludeDetail,
                                  bool AddEoln) {
  if (IncludeDetail)
    fputs("IntStream::Cursor<", File);
  fprintf(File, "@%" PRIxMAX "", uintmax_t(Index));
  if (Index == 0 && EnclosingBlocks.size() <= 1) {
    for (auto Pair : Stream->getHeader())
      fprintf(File, "{%" PRIxMAX ":%s}", uintmax_t(Pair.first),
              getName(Pair.second));
  }
  for (auto Blk : EnclosingBlocks)
    Blk->describe(File);
  if (IncludeDetail)
    fputc('>', File);
  if (AddEoln)
    fputc('\n', File);
  return File;
}

bool IntStream::Cursor::atEof() const {
  assert(!EnclosingBlocks.empty());
  return Index >= EnclosingBlocks.front()->getEndIndex();
}

bool IntStream::Cursor::atEob() const {
  assert(!EnclosingBlocks.empty());
  return Index >= EnclosingBlocks.back()->getEndIndex();
}

bool IntStream::Cursor::atEnd() const {
  return EnclosingBlocks.size() == 1 && atEof();
}

IntStream::BlockPtr IntStream::Cursor::closeBlock() {
  BlockPtr Blk;
  if (EnclosingBlocks.size() <= 1)
    return Blk;
  Blk = EnclosingBlocks.back();
  EnclosingBlocks.pop_back();
  return Blk;
}

bool IntStream::WriteCursor::write(IntType Value) {
  // TODO(karlschimpf): Add capability to communicate failure to caller.
  assert(!EnclosingBlocks.empty());
  assert(EnclosingBlocks.back()->getEndIndex() >= Index);
  Stream->Values.push_back(Value);
  ++Index;
  return true;
}

bool IntStream::WriteCursor::freezeEof() {
  if (Stream->isFrozen())
    return false;
  Stream->isFrozenFlag = true;
  size_t EofIndex = Stream->Values.size();
  for (auto Block : EnclosingBlocks)
    Block->EndIndex = EofIndex;
  return true;
}

bool IntStream::WriteCursor::openBlock() {
  assert(!EnclosingBlocks.empty());
  std::shared_ptr<Block> CurBlock = EnclosingBlocks.back();
  assert(CurBlock);
  auto Blk = std::make_shared<Block>(Index);
  CurBlock->Subblocks.push_back(Blk);
  EnclosingBlocks.push_back(Blk);
  assert(Stream);
  Stream->Blocks.push_back(Blk);
  return true;
}

bool IntStream::WriteCursor::closeBlock() {
  BlockPtr Blk = Cursor::closeBlock();
  if (!Blk)
    return false;
  Blk->EndIndex = Index;
  return true;
}

IntType IntStream::ReadCursor::read() {
  // TODO(karlschimpf): Add capability to communicate failure to caller.
  assert(!EnclosingBlocks.empty());
  assert(EnclosingBlocks.back()->getEndIndex() >= Index);
  assert(Index < Stream->Values.size());
  return Stream->Values[Index++];
}

bool IntStream::ReadCursor::openBlock() {
  if (NextBlock == EndBlocks)
    return false;
  BlockPtr Blk = *NextBlock;
  if (Index != Blk->getBeginIndex())
    return false;
  assert(!EnclosingBlocks.empty());
  std::shared_ptr<Block> CurBlock = EnclosingBlocks.back();
  assert(CurBlock);
  EnclosingBlocks.push_back(Blk);
  ++NextBlock;
  return true;
}

bool IntStream::ReadCursor::closeBlock() {
  BlockPtr Blk = Cursor::closeBlock();
  if (!Blk)
    return false;
  return Blk->getEndIndex() == Index;
}

void IntStream::reset() {
  Header.clear();
  Values.clear();
  TopBlock = std::make_shared<Block>();
  isFrozenFlag = false;
  Blocks.clear();
}

void IntStream::describe(FILE* File, const char* Name) {
  if (Name == nullptr)
    Name = "IntStream";
  fprintf(File, "*** %s ***\n", Name);
  fputs("Header:\n", File);
  for (auto Pair : Header) {
    fputs("  ", File);
    fprint_IntType(File, Pair.first);
    fprintf(File, " : %s\n", getName(Pair.second));
  }
  fputs("Blocks:\n", File);
  for (auto BlkIter = Blocks.begin(); BlkIter != Blocks.end(); ++BlkIter) {
    fputs("  ", File);
    BlkIter->get()->describe(File);
    fputc('\n', File);
  }
  fputs("Values:\n", File);
  size_t Index = 0;
  for (auto V : Values) {
    fprintf(File, "  [%" PRIxMAX "] ", Index);
    fprint_IntType(File, V);
    fputc('\n', File);
    ++Index;
  }
  fprintf(File, "******\n");
}

}  // end of namespace interp

}  // end of namespace wasm
