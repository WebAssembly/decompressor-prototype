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

// Runs some basic tests on class StreamReader.

// Note: Requires gtest from https://github.com/google/googletest

#include "stream/StringReader.h"
#include "gtest/gtest.h"

namespace {

class StreamReaderTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.
};

using namespace wasm;
using namespace wasm::decode;

TEST(StreamReaderTest, ReadBlock) {
  std::string Input("this is some text");
  static constexpr size_t OverflowSize = 10;
  for (size_t i = 0; i < Input.size()+OverflowSize; ++i) {
    StringReader Reader(Input);
    uint8_t Buffer[1024];
    assert(size(Buffer) >= Input.size() + OverflowSize);
    size_t Count = Reader.read(Buffer, i);
    if (i <= Input.size()) {
      EXPECT_EQ(i, Count) << "Did not fill reader as expected";
    } else {
      EXPECT_EQ(Count, Input.size()) << "Did not read entire string as expected";
    }
    for (size_t j = 0; j < Count; j++) {
      EXPECT_EQ(Buffer[j], uint8_t(Input[j])) << "Buffer filled incorrectly";
    }
  }
}
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
