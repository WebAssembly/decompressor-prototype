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

// Implements a string reader.

#include "stream/StringReader.h"

namespace wasm {

namespace decode {

StringReader::StringReader(std::string& Str) : Str(Str), Cursor(0) {
}

StringReader::~StringReader() {
}

AddressType StringReader::read(ByteType* Buf, AddressType Size) {
  if (Cursor >= Str.size())
    return 0;
  AddressType Available = Str.size() - Cursor;
  if (Available > Size)
    Available = Size;
  for (AddressType i = 0; i < Available; ++i)
    *(Buf++) = ByteType(Str[Cursor++]);
  return Available;
}

bool StringReader::write(ByteType* Buf, AddressType Size) {
  return false;
}

bool StringReader::freeze() {
  return true;
}

bool StringReader::atEof() {
  return Cursor >= Str.size();
}

}  // end of namespace decode

}  // end of namespace wasm
