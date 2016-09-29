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

#if 0
constexpr int32_t kEndOfStream = -1;
#endif

constexpr int32_t kDecodingError = -2;

struct Decompressor {
  Decompressor(const Decompressor& D) = delete;
  Decompressor& operator=(const Decompressor& D) = delete;

 public:
  uint8_t* InputBuffer;
  int32_t InputBufferSize;
  uint8_t* OutputBuffer;
  int32_t OutputBufferSize;
  std::shared_ptr<SymbolTable> Symtab;
  Decompressor();
  ~Decompressor();
  uint8_t* getNextInputBuffer(int32_t Size);
  int32_t resumeDecompression();
  uint8_t* getNextOutputBuffer(int32_t Size);
};
}

Decompressor::Decompressor()
    : InputBuffer(nullptr),
      InputBufferSize(0),
      OutputBuffer(nullptr),
      OutputBufferSize(0),
      Symtab(std::make_shared<SymbolTable>()) {
}

Decompressor::~Decompressor() {
  delete[] InputBuffer;
  delete[] OutputBuffer;
}

uint8_t* Decompressor::getNextInputBuffer(int32_t Size) {
  if (Size <= InputBufferSize)
    return InputBuffer;
  if (Size > InputBufferSize)
    delete[] InputBuffer;
  Size = std::max(Size, 1 >> 14);
  InputBuffer = new uint8_t[Size];
  InputBufferSize = Size;
  return InputBuffer;
}

int32_t Decompressor::resumeDecompression() {
  fatal("resumeDecompression not implemented!");
  return 0;
}

uint8_t* Decompressor::getNextOutputBuffer(int32_t Size) {
  fatal("getNextOutputBuffer notimplemented!");
  return nullptr;
}

}  // end of anonymous namespace

extern "C" {

Decompressor* create_decompressor() {
  auto* Decomp = new Decompressor();
  if (!SymbolTable::installPredefinedDefaults(Decomp->Symtab, false)) {
    delete Decomp;
    return nullptr;
  }
  return Decomp;
}

uint8_t* get_next_decompressor_input_buffer(Decompressor* D, int32_t Size) {
  return D->getNextInputBuffer(Size);
}

int32_t resume_decompression(Decompressor* D) {
  if (D == nullptr)
    return kDecodingError;
  return D->resumeDecompression();
}

uint8_t* get_next_decompressor_output_buffer(Decompressor* D, uint32_t Size) {
  return D->getNextOutputBuffer(Size);
}

void destroy_decompressor(Decompressor* D) {
  delete D;
}
}

}  // end of namespace interp

}  // end of namespace wasm
