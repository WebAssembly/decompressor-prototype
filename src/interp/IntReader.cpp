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

// Implementss a reader from a (non-file based) integer stream.

#include "interp/IntReader.h"

#include "interp/Interpreter.h"
#include "sexp/Ast.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

IntReader::IntReader(std::shared_ptr<IntStream> Input)
    : Reader(true),
      Pos(Input),
      Input(Input),
      HeaderIndex(0),
      StillAvailable(0),
      SavedPosStack(SavedPos) {
}

IntReader::~IntReader() {
}

TraceContextPtr IntReader::getTraceContext() {
  return Pos.getTraceContext();
}

namespace {

// Headroom is used to guarantee that several (integer) reads
// can be done in a single iteration of the loop.
constexpr size_t kResumeHeadroom = 100;

}  // end of anonymous namespace

bool IntReader::canProcessMoreInputNow() {
  StillAvailable = Pos.streamSize();
  if (!Input->isFrozen()) {
    if (StillAvailable < Pos.getIndex() + kResumeHeadroom)
      return false;
    StillAvailable -= kResumeHeadroom;
  }
  return true;
}

bool IntReader::stillMoreInputToProcessNow() {
  return Pos.getIndex() <= StillAvailable;
}

bool IntReader::atInputEob() {
  return Pos.atEob();
}

bool IntReader::atInputEof() {
  return Pos.atEof();
}

bool IntReader::pushPeekPos() {
  SavedPosStack.push(Pos);
  return true;
}

bool IntReader::popPeekPos() {
  if (SavedPosStack.empty())
    return false;
  Pos = SavedPos;
  SavedPosStack.pop();
  return true;
}

StreamType IntReader::getStreamType() {
  return StreamType::Int;
}

bool IntReader::processedInputCorrectly() {
  return Pos.atEnd();
}

bool IntReader::readBlockEnter() {
  return Pos.openBlock();
}

bool IntReader::readBlockExit() {
  return Pos.closeBlock();
}

void IntReader::readFillStart() {
}

void IntReader::readFillMoreInput() {
}

decode::IntType IntReader::read() {
  return Pos.read();
}

uint64_t IntReader::readVaruint64() {
  return read();
}

bool IntReader::readHeaderValue(IntTypeFormat Format, IntType& Value) {
  const IntStream::HeaderVector& Header = Input->getHeader();
  Value = 0;  // Default value for failure.
  if (HeaderIndex >= Header.size())
    return false;
  auto Pair = Header[HeaderIndex++];
  if (Pair.second != Format)
    return false;
  Value = Pair.first;
  return true;
}

bool IntReader::tablePush(IntType Value) {
  TableType::iterator Iter = Table.find(Value);
  if (Iter == Table.end()) {
    Table[Value] = Pos;
    TableRestoreFromSavedPos.push_back(false);
  } else {
    if (!pushPeekPos())
      return false;
    Pos = Iter->second;
    TableRestoreFromSavedPos.push_back(true);
  }
  return true;
}

bool IntReader::tablePop() {
  bool Restore = TableRestoreFromSavedPos.back();
  TableRestoreFromSavedPos.pop_back();
  if (Restore)
    if (!popPeekPos())
      return false;
  return true;
}

void IntReader::describePeekPosStack(FILE* File) {
  if (SavedPosStack.empty())
    return;
  fprintf(File, "*** Saved Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : SavedPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getIndex()));
  fprintf(File, "**********************\n");
}

}  // end of namespace interp

}  // end of namespace wasm
