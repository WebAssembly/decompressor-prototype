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
  std::shared_ptr<WriteCursor> InputPos;
  Pipe OutputPipe;
  std::shared_ptr<ReadCursor> OutputPos;
  std::shared_ptr<Interpreter> Interp;
  Decompressor();
  uint8_t* getBuffer(int32_t Size);
  int32_t resumeDecompression(int32_t Size);
  int32_t finishDecompression();
  bool fetchOutput(int32_t Size);
  int32_t currentStatus();
  int32_t currentOutputSize();

  TraceClassSexpReaderWriter& getTrace() { return Interp->getTrace(); }
  void describeInputPos() {
    fprintf(stderr, "InputPage %" PRIuMAX "[%" PRIxMAX ":%" PRIxMAX "]\n",
            uintmax_t(InputPos->getCurByteAddress()),
            uintmax_t(InputPos->getMinAddress()),
            uintmax_t(InputPos->getMaxAddress()));
  }
};

Decompressor::Decompressor()
    : BufferSize(0),
      Symtab(std::make_shared<SymbolTable>()),
      Input(std::make_shared<Queue>()) {
  InputPos = std::make_shared<WriteCursor>(Input);
  OutputPos = std::make_shared<ReadCursor>(OutputPipe.getOutput());
  describeInputPos();
}

uint8_t* Decompressor::getBuffer(int32_t Size) {
  TRACE_METHOD("get_decompressor_buffer");
  if (Size <= BufferSize)
    return Buffer.get();
  Buffer.reset(new uint8_t[Size]);
  BufferSize = Size;
  return Buffer.get();
}

int32_t Decompressor::currentStatus() {
  return (!Interp->isFinished() || Interp->isSuccessful())
      ? DECOMPRESSOR_SUCCESS : DECOMPRESSOR_ERROR;
}

int32_t Decompressor::currentOutputSize() {
  int32_t Status = currentStatus();
  if (Status == DECOMPRESSOR_SUCCESS)
    Status = OutputPipe.getOutput()->fillSize() - OutputPos->getCurByteAddress();
  TRACE(int32_t, "Status", Status);
  return Status;
}

int32_t Decompressor::resumeDecompression(int32_t Size) {
  TRACE_METHOD("resume_decompression");
  if (Size > BufferSize) {
    Interp->fail("resume_decompression(" + std::to_string(Size) +
                 "): illegal size");
    return DECOMPRESSOR_ERROR;
  }
  // TODO(karlschimpf) Speed up this copy.
  describeInputPos();
  for (int32_t i = 0; i < Size; ++i)
    InputPos->writeByte(Buffer.get()[i]);
  describeInputPos();
  Interp->resume();
  return currentOutputSize();
}

int32_t Decompressor::finishDecompression() {
  TRACE_METHOD("finish_decompression");
  Interp->fail("finishDecompression not implemented");
  return currentStatus();
}

bool Decompressor::fetchOutput(int32_t Size) {
  TRACE_METHOD("fetch_decompressor_output");
  Interp->fail("fetchOutput not implemented!");
  return currentStatus() == DECOMPRESSOR_SUCCESS;
}

}  // end of anonymous namespace

extern "C" {

void* create_decompressor() {
  auto* Decomp = new Decompressor();
  bool InstalledDefaults =
      SymbolTable::installPredefinedDefaults(Decomp->Symtab, false);
  Decomp->Interp = std::make_shared<Interpreter>(Decomp->Input,
                                                 Decomp->OutputPipe.getInput(),
                                                 Decomp->Symtab);
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
  return D->resumeDecompression(Size);
}

int32_t finish_decompression(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->finishDecompression();
}

bool fetch_decompressor_output(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->fetchOutput(Size);
}

void destroy_decompressor(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  delete D;
}

} // end extern "C".

}  // end of namespace interp

}  // end of namespace wasm
