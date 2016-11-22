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

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

IntReader::IntReader(IntStream::StreamPtr Input,
                     Writer& Output,
                     std::shared_ptr<filt::SymbolTable> Symtab)
    : Reader(Output, Symtab),
      Pos(Input),
      Input(Input),
      StillAvailable(0),
      PeekPosStack(PeekPos) {
}

IntReader::~IntReader() {
}

void IntReader::startAtBeginning() {
  Pos = IntStream::ReadCursor(Input);
  start();
}

namespace {

// Headroom is used to guarantee that several (integer) reads
// can be done in a single iteration of the loop.
constexpr size_t kResumeHeadroom = 100;

}  // end of anonymous namespace

bool IntReader::canProcessMoreInputNow() {
  StillAvailable = Pos.sizeAvailable();
  if (!Input->isFrozen()) {
    if (StillAvailable < kResumeHeadroom)
      return false;
    StillAvailable -= kResumeHeadroom;
  }
  return true;
}

bool IntReader::stillMoreInputToProcessNow() {
  return Pos.getIndex() < StillAvailable;
}

bool IntReader::atInputEob() {
  return Pos.atEob();
}

void IntReader::resetPeekPosStack() {
  PeekPos = IntStream::ReadCursor();
  PeekPosStack.clear();
}

void IntReader::pushPeekPos() {
  PeekPosStack.push(PeekPos);
}

void IntReader::popPeekPos() {
  Pos = PeekPos;
  PeekPosStack.pop();
}

StreamType IntReader::getStreamType() {
  return StreamType::Int;
}

bool IntReader::processedInputCorrectly() {
  return Pos.atEnd();
}

bool IntReader::enterBlock() {
  if (!Pos.openBlock()) {
    fail("Unable to enter block while reading");
    return false;
  }
  return true;
}

bool IntReader::exitBlock() {
  if (!Pos.closeBlock()) {
    fail("Unable to exit block while reading");
    return false;
  }
  return true;
}

void IntReader::readFillStart() {
}

void IntReader::readFillMoreInput() {
}

uint8_t IntReader::readUint8() {
  return Pos.read();
}

uint32_t IntReader::readUint32() {
  return Pos.read();
}

uint64_t IntReader::readUint64() {
  return Pos.read();
}

int32_t IntReader::readVarint32() {
  return Pos.read();
}

int64_t IntReader::readVarint64() {
  return Pos.read();
}

uint32_t IntReader::readVaruint32() {
  return Pos.read();
}

uint64_t IntReader::readVaruint64() {
  return Pos.read();
}

IntType IntReader::readValue(const filt::Node* Format) {
  // Note: We pass through virtual functions to force any applicable cast
  // conversions.
  switch (Format->getType()) {
    case OpUint32:
      return readUint32();
    case OpUint64:
      return readUint64();
    case OpUint8:
      return readUint8();
    case OpVarint32:
      return readVarint32();
    case OpVarint64:
      return readVarint64();
    case OpVaruint32:
      return readVaruint32();
    case OpVaruint64:
      return readVaruint64();
    default:
      fatal("readValue not defined for format!");
      return 0;
  }
}

void IntReader::describePeekPosStack(FILE* File) {
  if (PeekPosStack.empty())
    return;
  fprintf(File, "*** Peek Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : PeekPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getIndex()));
  fprintf(File, "**********************\n");
}

}  // end of namespace interp

}  // end of namespace wasm
