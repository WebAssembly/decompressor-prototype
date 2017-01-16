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

// Implements a stream reader for wasm/casm files.

#include "interp/StreamReader.h"

#include "interp/ByteReadStream.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

StreamReader::StreamReader(std::shared_ptr<decode::Queue> StrmInput)
    : ReadPos(StreamType::Byte, StrmInput),
      Input(std::make_shared<ByteReadStream>()),
      FillPos(0),
      PeekPosStack(PeekPos) {
}

StreamReader::~StreamReader() {
}

void StreamReader::setReadPos(const decode::ReadCursor& StartPos) {
  ReadPos = StartPos;
}

TraceClass::ContextPtr StreamReader::getTraceContext() {
  return ReadPos.getTraceContext();
}

namespace {

// Headroom is used to guarantee that several (integer) reads
// can be done in a single iteration of the loop.
constexpr size_t kResumeHeadroom = 100;

}  // end of anonymous namespace

bool StreamReader::canProcessMoreInputNow() {
  FillPos = ReadPos.fillSize();
  if (!ReadPos.isEofFrozen()) {
    if (FillPos < kResumeHeadroom)
      return false;
    FillPos -= kResumeHeadroom;
  }
  return true;
}

bool StreamReader::stillMoreInputToProcessNow() {
  return ReadPos.getCurByteAddress() <= FillPos;
}

ReadCursor& StreamReader::getPos() {
  return ReadPos;
}

bool StreamReader::atInputEob() {
  return ReadPos.atByteEob();
}

bool StreamReader::atInputEof() {
  return ReadPos.atEof();
}

void StreamReader::resetPeekPosStack() {
  PeekPos = ReadCursor();
  PeekPosStack.clear();
}

void StreamReader::pushPeekPos() {
  PeekPosStack.push(ReadPos);
}

void StreamReader::popPeekPos() {
  ReadPos = PeekPos;
  PeekPosStack.pop();
}

size_t StreamReader::sizePeekPosStack() {
  return PeekPosStack.size();
}

decode::StreamType StreamReader::getStreamType() {
  return Input->getType();
}

bool StreamReader::processedInputCorrectly() {
  return ReadPos.atEof() && ReadPos.isQueueGood();
}

bool StreamReader::readAction(const SymbolNode* Action) {
  switch (Action->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_enter_readonly: {
      const uint32_t OldSize = Input->readBlockSize(ReadPos);
      TRACE(uint32_t, "block size", OldSize);
      Input->pushEobAddress(ReadPos, OldSize);
      return true;
    }
    case PredefinedSymbol::Block_exit:
    case PredefinedSymbol::Block_exit_readonly:
      ReadPos.popEobAddress();
      return true;
    default:
      return true;
  }
}

void StreamReader::readFillStart() {
  FillCursor = ReadPos;
}

void StreamReader::readFillMoreInput() {
  if (FillCursor.atEof())
    return;
  FillCursor.advance(Page::Size);
}

uint8_t StreamReader::readUint8() {
  return Input->readUint8(ReadPos);
}

uint32_t StreamReader::readUint32() {
  return Input->readUint32(ReadPos);
}

uint64_t StreamReader::readUint64() {
  return Input->readUint64(ReadPos);
}

int32_t StreamReader::readVarint32() {
  return Input->readVarint32(ReadPos);
}

int64_t StreamReader::readVarint64() {
  return Input->readVarint64(ReadPos);
}

uint32_t StreamReader::readVaruint32() {
  return Input->readVaruint32(ReadPos);
}

uint64_t StreamReader::readVaruint64() {
  return Input->readVaruint64(ReadPos);
}

void StreamReader::describePeekPosStack(FILE* File) {
  if (PeekPosStack.empty())
    return;
  fprintf(File, "*** Peek Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : PeekPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "**********************\n");
}

}  // end of namespace interp

}  // end of namespace wasm
