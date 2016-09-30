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

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

extern "C" {

namespace {

struct Decompressor {
  Decompressor(const Decompressor& D) = delete;
  Decompressor& operator=(const Decompressor& D) = delete;

 public:
  uint8_t* InputBuffer;
  int32_t InputBufferSize;
  int32_t InputBufferAllocSize;
  uint8_t* OutputBuffer;
  int32_t OutputBufferSize;
  int32_t OutputBufferAllocSize;
  bool Broken;
  std::shared_ptr<SymbolTable> Symtab;
  std::shared_ptr<Queue> Input;
  std::shared_ptr<ReadCursor> ReadPos;
  std::shared_ptr<Queue> Output;
  std::shared_ptr<WriteCursor> WritePos;
  std::shared_ptr<Interpreter> Interp;
  Decompressor();
  ~Decompressor();
  uint8_t* getNextInputBuffer(int32_t Size);
  int32_t resumeDecompression();
  int32_t finishDecompression();
  uint8_t* getNextOutputBuffer(int32_t Size);
  int32_t currentStatus();
};
}

Decompressor::Decompressor()
    : InputBuffer(nullptr),
      InputBufferSize(0),
      InputBufferAllocSize(0),
      OutputBuffer(nullptr),
      OutputBufferSize(0),
      OutputBufferAllocSize(0),
      Broken(false),
      Symtab(std::make_shared<SymbolTable>()),
      Input(std::make_shared<Queue>()),
      Output(std::make_shared<Queue>()) {
  ReadPos = std::make_shared<ReadCursor>(Input);
  WritePos = std::make_shared<WriteCursor>(Output);
}

Decompressor::~Decompressor() {
  delete[] InputBuffer;
  delete[] OutputBuffer;
}

uint8_t* Decompressor::getNextInputBuffer(int32_t Size) {
  if (Size <= InputBufferAllocSize) {
    InputBufferSize = Size;
    return InputBuffer;
  }
  if (Size > InputBufferAllocSize)
    delete[] InputBuffer;
  InputBufferAllocSize = std::max(Size, 1 >> 14);
  InputBuffer = new uint8_t[InputBufferAllocSize];
  InputBufferSize = Size;
  return InputBuffer;
}

int32_t Decompressor::currentStatus() {
  fatal("currentStatus not implemented");
  return DECOMPRESSOR_ERROR;
}

int32_t Decompressor::resumeDecompression() {
  if (Interp->isFinished())
    return currentStatus();
  Interp->resume();
  return currentStatus();
}

int32_t Decompressor::finishDecompression() {
  fatal("finishDecompression not implemented!");
  return 0;
}

uint8_t* Decompressor::getNextOutputBuffer(int32_t Size) {
  fatal("getNextOutputBuffer notimplemented!");
  return nullptr;
}

}  // end of anonymous namespace

extern "C" {

void* create_decompressor() {
  auto* Decomp = new Decompressor();
  if (!SymbolTable::installPredefinedDefaults(Decomp->Symtab, false)) {
    Decomp->Broken = true;
    return Decomp;
  }
  Decomp->Interp = std::make_shared<Interpreter>(Decomp->Input,
                                                 Decomp->Output,
                                                 Decomp->Symtab);
  Decomp->Interp->setTraceProgress(true);
  return Decomp;
}

void* get_next_decompressor_input_buffer(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->getNextInputBuffer(Size);
}

int32_t resume_decompression(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  if (D->Broken)
    return DECOMPRESSOR_ERROR;
  return D->resumeDecompression();
}

int32_t finish_decompression(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  if (D->Broken)
    return DECOMPRESSOR_ERROR;
  return D->finishDecompression();
}

void* get_next_decompressor_output_buffer(void* Dptr, int32_t Size) {
  Decompressor* D = (Decompressor*)Dptr;
  return D->getNextOutputBuffer(Size);
}

void destroy_decompressor(void* Dptr) {
  Decompressor* D = (Decompressor*)Dptr;
  delete D;
}
}

}  // end of namespace interp

}  // end of namespace wasm
