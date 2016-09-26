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

namespace interp {

extern "C" {

struct Decompressor {
  Decompressor(const Decompressor& D) = delete;
  Decompressor& operator=(const Decompressor& D) = delete;

 public:
  uint8_t* InputBuffer;
  int32_t InputSize;
  uint8_t* OutputBuffer;
  int32_t OutputSize;
  Decompressor();
  ~Decompressor();
  uint8_t* getNextInputBuffer(int32_t Size);  /* TODO */
  int32_t resumeDecompression();              /* TODO */
  uint8_t* getNextOutputBuffer(int32_t Size); /* TODO */
};
}

Decompressor::Decompressor()
    : InputBuffer(nullptr), InputSize(0), OutputBuffer(nullptr), OutputSize(0) {
}

Decompressor::~Decompressor() {
  delete[] InputBuffer;
  delete[] OutputBuffer;
}

extern "C" {

Decompressor* create_decompressor() {
  return new Decompressor();
}

uint8_t* get_next_decompressor_input_buffer(Decompressor* D, int32_t Size) {
  return D->getNextInputBuffer(Size);
}

int32_t resume_decompression(Decompressor* D) {
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
