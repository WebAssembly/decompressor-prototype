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

namespace interp {

IntStream::Cursor::Cursor(StreamPtr Stream) : Index(0), Stream(Stream) {
  assert(Stream);
  EnclosingBlocks.push_back(Stream->TopBlock);
}

IntStream::Cursor::Cursor(const IntStream::Cursor& C)
    : Index(C.Index), EnclosingBlocks(C.EnclosingBlocks), Stream(C.Stream) {
}

IntStream::Cursor& IntStream::Cursor::operator=(const IntStream::Cursor& C) {
  Index = C.Index;
  EnclosingBlocks = C.EnclosingBlocks;
  Stream = C.Stream;
  return *this;
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
  return true;
}

bool IntStream::ReadCursor::closeBlock() {
  BlockPtr Blk = Cursor::closeBlock();
  if (!Blk)
    return false;
  return Blk->getEndIndex() == Index;
}

}  // end of namespace interp

}  // end of namespace wasm
