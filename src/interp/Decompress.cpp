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

// Implementation of the C API to the decompressor interpreter.

#include "interp/Decompress.h"
#include "interp/Interpreter.h"
#include "stream/Pipe.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

namespace {

struct Decompressor {
  Decompressor(const Decompressor& D) = delete;
  Decompressor& operator=(const Decompressor& D) = delete;

 public:
  enum class State { NeedsMoreInput, FlushingOutput, Succeeded, Failed };
  std::unique_ptr<uint8_t> Buffer;
  int32_t BufferSize;
  std::shared_ptr<SymbolTable> Symtab;
  std::shared_ptr<Queue> Input;
  std::shared_ptr<WriteCursor2ReadQueue> InputPos;
  Pipe OutputPipe;
  std::shared_ptr<ReadCursor> OutputPos;
  std::shared_ptr<Interpreter> Interp;
  State MyState;
  Decompressor();
  uint8_t* getBuffer(int32_t Size);
  int32_t resume(int32_t Size);
  void closeInput();
  bool fetchOutput(int32_t Size);
  int32_t getOutputSize() {
    return OutputPipe.getOutput()->fillSize() - OutputPos->getCurByteAddress();
  }
  TraceClassSexp& getTrace() { return Interp->getTrace(); }
  void setTraceProgress(bool NewValue) { Interp->setTraceProgress(NewValue); }

 private:
  int32_t flushOutput();
  int32_t fail() {
    MyState = State::Failed;
    return DECOMPRESSOR_ERROR;
  }
};

Decompressor::Decompressor()
    : BufferSize(0),
      Symtab(std::make_shared<SymbolTable>()),
      Input(std::make_shared<Queue>()),
      MyState(State::NeedsMoreInput) {
  fprintf(stderr, "Builing inputpos...\n");
  InputPos = std::make_shared<WriteCursor2ReadQueue>(Input);
  fprintf(stderr, "Builing outputpos...\n");
  OutputPos = std::make_shared<ReadCursor>(OutputPipe.getOutput());
}

uint8_t* Decompressor::getBuffer(int32_t Size) {
  TRACE_METHOD("get_decompressor_buffer");
  TRACE(bool, "AtEof", InputPos->atEof());
  if (Size <= BufferSize)
    return Buffer.get();
  Buffer.reset(new uint8_t[Size]);
  BufferSize = Size;
  return Buffer.get();
}

int32_t Decompressor::flushOutput() {
  TRACE(int32_t, "OutputSize", getOutputSize());
  if (int32_t OutputSize = getOutputSize())
    return OutputSize;
  MyState = State::Succeeded;
  return DECOMPRESSOR_SUCCESS;
}

int32_t Decompressor::resume(int32_t Size) {
  TRACE_METHOD("resume_decompression");
  switch (MyState) {
    case State::NeedsMoreInput:
      if (Size == 0) {
        if (!InputPos->atEof()) {
          TRACE_MESSAGE("Closing input");
          InputPos->freezeEof();
          InputPos->close();
        }
      } else {
        if (InputPos->atEof()) {
          Interp->fail("resume_decompression(" + std::to_string(Size) +
                       "): can't add bytes when input closed");
          return fail();
        }
        if (Size > BufferSize) {
          Interp->fail("resume_decompression(" + std::to_string(Size) +
                       "): illegal size");
          return fail();
        }
        // TODO(karlschimpf) Speed up this copy.
        uint8_t* Buf = Buffer.get();
        for (int32_t i = 0; i < Size; ++i)
          InputPos->writeByte(Buf[i]);
      }
      Interp->resume();
      if (Interp->errorsFound())
        return fail();
      if (!Interp->isFinished())
        return getOutputSize();
      OutputPipe.getInput()->close();
      if (!Interp->isSuccessful())
        return fail();
      MyState = State::FlushingOutput;
      return flushOutput();
    case State::FlushingOutput:
      return flushOutput();
    case State::Succeeded:
      if (Size == 0)
        return DECOMPRESSOR_SUCCESS;
      Interp->fail("resume_decompression(" + std::to_string(Size) +
                   "): can't add bytes when input closed");
      break;
    case State::Failed:
      break;
  }
  return DECOMPRESSOR_ERROR;
}

bool Decompressor::fetchOutput(int32_t Size) {
  TRACE_METHOD("fetch_decompressor_output");
  switch (MyState) {
    case State::Succeeded:
    case State::Failed:
      return false;
    default:
      break;
  }
  if (Size > BufferSize || Size > getOutputSize()) {
    fail();
    return false;
  }
  // TODO(karlschimpf): Do this more efficiently.
  uint8_t* Buf = Buffer.get();
  for (int32_t i = 0; i < Size; ++i)
    Buf[i] = OutputPos->readByte();
  return Size;
}

}  // end of anonymous namespace

extern "C" {

void* create_decompressor() {
  auto* Decomp = new Decompressor();
  bool InstalledDefaults =
      SymbolTable::installPredefinedDefaults(Decomp->Symtab, false);
  Decomp->Interp = std::make_shared<Interpreter>(
      Decomp->Input, Decomp->OutputPipe.getInput(), Decomp->Symtab);
  // Decomp->Interp->setTraceProgress(true);
  Decomp->Interp->start();
  if (!InstalledDefaults)
    Decomp->Interp->fail("Unable to install decompression rules!");
  return Decomp;
}

void set_trace_decompression(void* Dptr, bool NewValue) {
  Decompressor* D = (Decompressor*)Dptr;
  D->setTraceProgress(NewValue);
}

uint8_t* get_decompressor_buffer(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->getBuffer(Size);
}

int32_t resume_decompression(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->resume(Size);
}

bool fetch_decompressor_output(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->fetchOutput(Size);
}

void destroy_decompressor(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  delete D;
}

}  // end extern "C".

}  // end of namespace interp

}  // end of namespace wasm
