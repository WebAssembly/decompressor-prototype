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
  std::unique_ptr<uint8_t> Buffer;
  int32_t BufferSize;
  std::shared_ptr<SymbolTable> Symtab;
  std::shared_ptr<Queue> Input;
  std::shared_ptr<WriteCursor2ReadQueue> InputPos;
  Pipe OutputPipe;
  std::shared_ptr<ReadCursor> OutputPos;
  std::shared_ptr<Interpreter> Interp;
  Decompressor();
  uint8_t* getBuffer(int32_t Size);
  int32_t resume(int32_t Size);
  void closeInput();
  bool fetchOutput(int32_t Size);
  int32_t getOutputSize() {
    return OutputPipe.getOutput()->fillSize() - OutputPos->getCurByteAddress();
  }
  TraceClassSexpReaderWriter& getTrace() { return Interp->getTrace(); }
};

Decompressor::Decompressor()
    : BufferSize(0),
      Symtab(std::make_shared<SymbolTable>()),
      Input(std::make_shared<Queue>()) {
  InputPos = std::make_shared<WriteCursor2ReadQueue>(Input);
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

int32_t Decompressor::resume(int32_t Size) {
  TRACE_METHOD("resume_decompression");
  if (Size > BufferSize) {
    Interp->fail("resume_decompression(" + std::to_string(Size) +
                 "): illegal size");
    return DECOMPRESSOR_ERROR;
  }
  if (InputPos->atEof()) {
    if (Size > 0) {
      Interp->fail("resume_decompression(" + std::to_string(Size) +
                   "): can't add bytes when input closed");
      return DECOMPRESSOR_ERROR;
    }
  } else if (Size == 0) {
    TRACE_MESSAGE("Closing input");
    InputPos->freezeEof();
  } else {
    // TODO(karlschimpf) Speed up this copy.
    for (int32_t i = 0; i < Size; ++i)
      InputPos->writeByte(Buffer.get()[i]);
  }
  Interp->resume();
  if (Interp->errorsFound())
    return DECOMPRESSOR_ERROR;
  if (!Interp->isFinished())
    return getOutputSize();
  if (!Interp->isSuccessful())
    return DECOMPRESSOR_ERROR;
  TRACE(int32_t, "OutputSize", getOutputSize());
  if (int32_t OutputSize = getOutputSize())
    return OutputSize;
  return DECOMPRESSOR_SUCCESS;
}

bool Decompressor::fetchOutput(int32_t Size) {
  TRACE_METHOD("fetch_decompressor_output");
  Interp->fail("fetchOutput not implemented!");
  return false;
}

}  // end of anonymous namespace

extern "C" {

void* create_decompressor() {
  auto* Decomp = new Decompressor();
  bool InstalledDefaults =
      SymbolTable::installPredefinedDefaults(Decomp->Symtab, false);
  Decomp->Interp = std::make_shared<Interpreter>(
      Decomp->Input, Decomp->OutputPipe.getInput(), Decomp->Symtab);
  Decomp->Interp->setTraceProgress(true);
  Decomp->Interp->start();
  if (!InstalledDefaults)
    Decomp->Interp->fail("Unable to install decompression rules!");
  return Decomp;
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
