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

#include "interp/ByteReader.h"

#include "interp/ByteReadStream.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

ByteReader::ByteReader(std::shared_ptr<decode::Queue> StrmInput)
    : ReadPos(StreamType::Byte, StrmInput),
      Input(std::make_shared<ByteReadStream>()),
      FillPos(0),
      PeekPosStack(PeekPos) {
}

ByteReader::~ByteReader() {
}

void ByteReader::setReadPos(const decode::ReadCursor& StartPos) {
  ReadPos = StartPos;
}

TraceClass::ContextPtr ByteReader::getTraceContext() {
  return ReadPos.getTraceContext();
}

namespace {

// Headroom is used to guarantee that several (integer) reads
// can be done in a single iteration of the loop.
constexpr size_t kResumeHeadroom = 100;

}  // end of anonymous namespace

bool ByteReader::canProcessMoreInputNow() {
  FillPos = ReadPos.fillSize();
  if (!ReadPos.isEofFrozen()) {
    if (FillPos < ReadPos.getCurAddress() + kResumeHeadroom)
      return false;
    FillPos -= kResumeHeadroom;
  }
  return true;
}

bool ByteReader::stillMoreInputToProcessNow() {
  return ReadPos.getCurAddress() <= FillPos;
}

ReadCursor& ByteReader::getPos() {
  return ReadPos;
}

bool ByteReader::atInputEob() {
  return ReadPos.atEob();
}

bool ByteReader::atInputEof() {
  return ReadPos.atEof();
}

void ByteReader::resetPeekPosStack() {
  PeekPos = ReadCursor();
  PeekPosStack.clear();
}

void ByteReader::pushPeekPos() {
  PeekPosStack.push(ReadPos);
}

void ByteReader::popPeekPos() {
  ReadPos = PeekPos;
  PeekPosStack.pop();
}

size_t ByteReader::sizePeekPosStack() {
  return PeekPosStack.size();
}

decode::StreamType ByteReader::getStreamType() {
  return Input->getType();
}

bool ByteReader::processedInputCorrectly() {
  return ReadPos.atEof() && ReadPos.isQueueGood();
}

bool ByteReader::readAction(const SymbolNode* Action) {
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

void ByteReader::readFillStart() {
  FillCursor = ReadPos;
}

void ByteReader::readFillMoreInput() {
  if (FillCursor.atEof())
    return;
  FillCursor.advance(Page::Size);
}

uint8_t ByteReader::readUint8() {
  return Input->readUint8(ReadPos);
}

uint32_t ByteReader::readUint32() {
  return Input->readUint32(ReadPos);
}

uint64_t ByteReader::readUint64() {
  return Input->readUint64(ReadPos);
}

int32_t ByteReader::readVarint32() {
  return Input->readVarint32(ReadPos);
}

int64_t ByteReader::readVarint64() {
  return Input->readVarint64(ReadPos);
}

uint32_t ByteReader::readVaruint32() {
  return Input->readVaruint32(ReadPos);
}

uint64_t ByteReader::readVaruint64() {
  return Input->readVaruint64(ReadPos);
}

void ByteReader::describePeekPosStack(FILE* File) {
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
