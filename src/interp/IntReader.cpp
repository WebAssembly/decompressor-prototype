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

#include "interp/Algorithm.h"
#include "interp/IntAlgorithm.h"
#include "interp/IntFormats.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

IntReader::IntReader(std::shared_ptr<IntStream> Input)
    : Pos(Input),
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

size_t IntReader::sizePeekPosStack() {
  return PeekPosStack.size();
}

StreamType IntReader::getStreamType() {
  return StreamType::Int;
}

bool IntReader::processedInputCorrectly() {
  return Pos.atEnd();
}

bool IntReader::readAction(const SymbolNode* Action) {
  switch (Action->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_enter_readonly:
      return Pos.openBlock();
    case PredefinedSymbol::Block_exit:
    case PredefinedSymbol::Block_exit_readonly:
      return Pos.closeBlock();
    default:
      return true;
  }
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

void IntReader::describePeekPosStack(FILE* File) {
  if (PeekPosStack.empty())
    return;
  fprintf(File, "*** Peek Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : PeekPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getIndex()));
  fprintf(File, "**********************\n");
}

IntStructureReader::IntStructureReader(
    std::shared_ptr<IntReader> Input,
    std::shared_ptr<Writer> Output,
    std::shared_ptr<filt::SymbolTable> Symtab)
    : Reader(Input, Output, Symtab), IntInput(Input) {
  // TODO(karlschimpf) Modify structuralStart() to mimic algorithmStart(),
  // except that it calls structuralResume() to remove this assertion.
  assert(Symtab &&
         "IntStructureReader must be given algorithm at construction");
}

IntStructureReader::~IntStructureReader() {
}

const char* IntStructureReader::getDefaultTraceName() const {
  return "IntReader";
}

void IntStructureReader::structuralStart() {
  algorithmStart();
}

void IntStructureReader::structuralResume() {
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
            for (auto Pair : IntInput->getStream()->getHeader()) {
              IntType Value;
              if (!readHeaderValue(Pair.second, Value)) {
                TRACE(IntType, "Lit Value", Pair.first);
                TRACE(string, "Lit format", wasm::interp::getName(Pair.second));
                return throwMessage("unable to read header literal");
              }
              if (Value != Pair.first)
                return throwBadHeaderValue(Pair.first, Value,
                                           ValueFormat::Hexidecimal);
              Output->writeHeaderValue(Pair.first, Pair.second);
            }
            LocalValues.push_back(IntInput->getStream()->size());
            Frame.CallState = State::Exit;
            call(Method::ReadIntBlock, Frame.CallModifier, nullptr);
            break;
          case State::Exit:
            if (FreezeEofAtExit && !Output->writeFreezeEof())
              return throwCantFreezeEof();
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
            if (Input->atInputEob()) {
              Frame.CallState = State::Exit;
              break;
            }
            // Check if any nested blocks.
            bool hasNestedBlocks = IntInput->hasMoreBlocks();
            if (hasNestedBlocks) {
              IntStream::BlockPtr Blk = IntInput->getNextBlock();
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
            IntStream::BlockPtr Blk = IntInput->getNextBlock();
            LocalValues.push_back(Blk->getBeginIndex());
            Frame.CallState = State::Step2;
            call(Method::ReadIntValues, Frame.CallModifier, nullptr);
            break;
          }
          case State::Step2: {
            // At the beginning of a nested block.
            IntStream::BlockPtr Blk = IntInput->getNextBlock();
            TRACE_BLOCK(
                { TRACE(hex_size_t, "block.open", Blk->getBeginIndex()); });
            SymbolNode* EnterBlock =
                Symtab->getPredefined(PredefinedSymbol::Block_enter);
            if (!Input->readAction(EnterBlock) ||
                !Output->writeAction(EnterBlock))
              return fatal("Unable to enter block");
            Frame.CallState = State::Step3;
            LocalValues.push_back(Blk->getEndIndex());
            call(Method::ReadIntBlock, Frame.CallModifier, nullptr);
            break;
          }
          case State::Step3: {
            // At the end of a nested block.
            TRACE_BLOCK(
                { TRACE(hex_size_t, "block.close", LocalValues.back()); });
            SymbolNode* ExitBlock =
                Symtab->getPredefined(PredefinedSymbol::Block_exit);
            if (!Input->readAction(ExitBlock) ||
                !Output->writeAction(ExitBlock))
              return fatal("unable to close block");
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
            if (IntInput->getIndex() >= EndIndex) {
              Frame.CallState = State::Exit;
              break;
            }
            IntType Value = IntInput->read();
            TRACE(IntType, "value", Value);
            if (!Output->writeVarint64(Value))
              return throwMessage("Unable to write last value");
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

void IntStructureReader::structuralReadBackFilled() {
  readFillStart();
  while (!isFinished()) {
    readFillMoreInput();
    structuralResume();
  }
}

}  // end of namespace interp

}  // end of namespace wasm
