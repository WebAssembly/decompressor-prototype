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
using namespace utils;

namespace interp {

IntReader::IntReader(IntStream::StreamPtr Input,
                     Writer& Output,
                     std::shared_ptr<filt::SymbolTable> Symtab)
    : Reader(Output, Symtab),
      Pos(Input),
      Input(Input),
      HeaderIndex(0),
      StillAvailable(0),
      PeekPosStack(PeekPos) {
}

IntReader::~IntReader() {
}

TraceClass::ContextPtr IntReader::getTraceContext() {
  return Pos.getTraceContext();
}

const char* IntReader::getDefaultTraceName() const {
  return "IntReader";
}

bool IntReader::canFastRead() const {
  return true;
}

void IntReader::fastStart() {
  algorithmStart();
}

void IntReader::fastResume() {
  if (!canProcessMoreInputNow())
    return;
  while (stillMoreInputToProcessNow()) {
    if (errorsFound())
      break;
    switch (Frame.CallMethod) {
      default:
        return handleOtherMethods();
      case Method::GetFile:
        switch (Frame.CallState) {
          case State::Enter:
            for (auto Pair : Input->getHeader()) {
              IntType Value = readHeaderValue(Pair.second);
              if (Value != Pair.first)
                return failBadHeaderValue(Pair.first, Value,
                                          ValueFormat::Hexidecimal);
              Output.writeHeaderValue(Pair.first, Pair.second);
            }
            LocalValues.push_back(Input->size());
            Frame.CallState = State::Exit;
            call(Method::ReadIntBlock, Frame.CallModifier, nullptr);
            break;
          case State::Exit:
            if (FreezeEofAtExit && !Output.writeFreezeEof())
              return failFreezingEof();
            popAndReturn();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::ReadIntBlock:
        switch (Frame.CallState) {
          case State::Enter:
            Frame.CallState = State::Loop;
            break;
          case State::Loop: {
            // Check if end of current (enclosing) block has been reached.
            size_t Eob = LocalValues.back();
            if (Pos.getIndex() >= Eob) {
              Frame.CallState = State::Exit;
              break;
            }
            // Check if any nested blocks.
            bool hasNestedBlocks = Pos.hasMoreBlocks();
            if (hasNestedBlocks) {
              IntStream::BlockPtr Blk = Pos.getNextBlock();
              if (Blk->getBeginIndex() >= Eob)
                hasNestedBlocks = false;
            }
            if (!hasNestedBlocks) {
              // Only top-level values left. Read until all processed.
              LocalValues.push_back(Eob);
              Frame.CallState = State::Exit;
              call(Method::ReadIntValues, Frame.CallModifier, nullptr);
              break;
            }
            // Read to beginning of nested block.
            IntStream::BlockPtr Blk = Pos.getNextBlock();
            LocalValues.push_back(Blk->getBeginIndex());
            Frame.CallState = State::Step2;
            call(Method::ReadIntValues, Frame.CallModifier, nullptr);
            break;
          }
          case State::Step2: {
            // At the beginning of a nested block.
            IntStream::BlockPtr Blk = Pos.getNextBlock();
            TRACE_BLOCK(
                { TRACE(hex_size_t, "block.open", Blk->getBeginIndex()); });
            if (!enterBlock())
              return fail("Unable to open block");
            if (!Output.writeAction(Symtab->getBlockEnterCallback()))
              break;
            Frame.CallState = State::Step3;
            LocalValues.push_back(Blk->getEndIndex());
            call(Method::ReadIntBlock, Frame.CallModifier, nullptr);
            break;
          }
          case State::Step3: {
            // At the end of a nested block.
            TRACE_BLOCK(
                { TRACE(hex_size_t, "block.close", LocalValues.back()); });
            if (!Output.writeAction(Symtab->getBlockExitCallback()))
              break;
            if (!exitBlock())
              return fail("Unable to close block");
            // Continue to process rest of block.
            Frame.CallState = State::Loop;
            break;
          }
          case State::Exit:
            LocalValues.pop_back();
            popAndReturn();
            break;
          default:
            return failBadState();
        }
        break;
      case Method::ReadIntValues:
        switch (Frame.CallState) {
          case State::Enter:
            Frame.CallState = State::Loop;
            break;
          case State::Loop: {
            size_t EndIndex = LocalValues.back();
            if (Pos.getIndex() >= EndIndex) {
              Frame.CallState = State::Exit;
              break;
            }
            IntType Value = read();
            TRACE(IntType, "value", Value);
            if (!Output.writeVarint64(Value))
              return fail("Unable to write last value");
            break;
          }
          case State::Exit:
            LocalValues.pop_back();
            popAndReturn();
            break;
          default:
            return failBadState();
        }
        break;
    }
  }
}

void IntReader::fastReadBackFilled() {
  readFillStart();
  while (!isFinished()) {
    readFillMoreInput();
    fastResume();
  }
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
  return Pos.getIndex() <= StillAvailable;
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

decode::IntType IntReader::read() {
  return Pos.read();
}

uint8_t IntReader::readUint8() {
  return read();
}

uint32_t IntReader::readUint32() {
  return read();
}

uint64_t IntReader::readUint64() {
  return read();
}

int32_t IntReader::readVarint32() {
  return read();
}

int64_t IntReader::readVarint64() {
  return read();
}

uint32_t IntReader::readVaruint32() {
  return read();
}

uint64_t IntReader::readVaruint64() {
  return read();
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

IntType IntReader::readHeaderValue(IntTypeFormat Format) {
  const IntStream::HeaderVector& Header = Input->getHeader();
  if (HeaderIndex >= Header.size()) {
    fail("Header value expected, but not found");
    return 0;
  }
  auto Pair = Header[HeaderIndex++];
  if (Pair.second != Format) {
    fail(std::string("Expected header value of type ") +
         interp::getName(Format) + " but found " +
         interp::getName(Pair.second));
  }
  TRACE(hex_IntType, "Value", Pair.first);
  return Pair.first;
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
