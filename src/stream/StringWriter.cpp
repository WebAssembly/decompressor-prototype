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

// Implements a string writer.

#include "stream/StringWriter.h"

namespace wasm {

namespace decode {

size_t StringWriter::read(uint8_t*, size_t) { return 0; }

bool StringWriter::write(uint8_t* Buf, size_t Size) {
  if (IsFrozen)
    return false;
  for (size_t i = 0; i < Size; ++i)
    Str.push_back(*(Buf++));
  return true;
}

bool StringWriter::freeze() {
  return IsFrozen = true;
}

bool StringWriter::atEof() {
  return IsFrozen;
}

}  // end of namespace decode

}  // end of namespace wasm
