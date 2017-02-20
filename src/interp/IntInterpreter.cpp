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

#include "interp/IntInterpreter.h"

#include "interp/IntReader.h"
#include "interp/IntStream.h"
#include "interp/Writer.h"
#include "sexp/Ast.h"
#include "utils/Trace.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

IntInterpreter::IntInterpreter(std::shared_ptr<IntReader> Input,
                               std::shared_ptr<Writer> Output,
                               const InterpreterFlags& Flags,
                               std::shared_ptr<filt::SymbolTable> Symtab)
    : Interpreter(Input, Output, Flags, Symtab), IntInput(Input) {
}

IntInterpreter::~IntInterpreter() {
}

const char* IntInterpreter::getDefaultTraceName() const {
  return "IntReader";
}

void IntInterpreter::structuralStart() {
  assert(Symtab && "IntInterpreter must be given algorithm at construction");
  return callTopLevel(Method::GetFile, nullptr);
}

void IntInterpreter::structuralRead() {
  structuralStart();
  structuralReadBackFilled();
}
void IntInterpreter::structuralResume() {
  if (!Input->canProcessMoreInputNow())
    return;
  while (Input->stillMoreInputToProcessNow()) {
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
              if (!Input->readHeaderValue(Pair.second, Value)) {
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

void IntInterpreter::structuralReadBackFilled() {
  Input->readFillStart();
  while (!isFinished()) {
    Input->readFillMoreInput();
    structuralResume();
  }
}

}  // end of namespace interp

}  // end of namespace wasm
