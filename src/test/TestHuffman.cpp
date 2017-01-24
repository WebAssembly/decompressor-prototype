/* -*- C++ -*- */
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

// Simple tests of huffman encoding.

#include "utils/ArgsParse.h"
#include "utils/Defs.h"
#include "utils/HuffmanEncoding.h"

using namespace wasm;
using namespace wasm::utils;

namespace {

} // end of anonymous namespace

void TestEncoding(const char* Title,
                  unsigned MaxPathLength,
                  HuffmanEncoder::WeightType* Weights,
                  size_t WeightsSize) {
  fprintf(stdout, "Test %s: max path length = %u\n", Title, MaxPathLength);
  std::shared_ptr<HuffmanEncoder> Encoder = std::make_shared<HuffmanEncoder>();
  Encoder->setMaxPathLength(MaxPathLength);
  fprintf(stdout, "Creating Symbols:\n");
  for (size_t i = 0; i < WeightsSize; ++i)
    Encoder->createSymbol(Weights[i])->describe(stdout);
  fprintf(stdout, "Huffman encoding:\n");
  Encoder->encodeSymbols()->describe(stdout, false);
}

HuffmanEncoder::WeightType Weights1[] = {
  10,
  1,
  100,
  5,
  9,
  54,
  150
};

HuffmanEncoder::WeightType Weights2[] = {
  1,
  1,
  1,
  2,
  2,
  3,
  5,
  7,
  9,
  11,
  15,
  25,
  32,
  38,
  64,
  69,
  75,
  101,
  105,
  110,
  150,
  200,
  600,
  1201,
  1503,
  4200,
  4600,
  7012,
  10000,
  11000,
  13000,
  14000,
  20000,
};

int main(int Argc, const char* Argv[]) {
  TestEncoding("Weights1", 32, Weights1, size(Weights1));
  TestEncoding("Weights1", 3, Weights1, size(Weights1));
  TestEncoding("Weights2", 32, Weights2, size(Weights2));
  TestEncoding("Weights2", 6, Weights2, size(Weights2));
  return 0;
}
