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

// Defines a string stream for reading.

#ifndef DECOMPRESSOR_SRC_STREAM_STRINGREADER_H
#define DECOMPRESSOR_SRC_STREAM_STRINGREADER_H

#include "stream/RawStream.h"

#include <string>

namespace wasm {

namespace decode {

class StringReader : public RawStream {
  StringReader() = delete;
  StringReader(const StringReader&) = delete;
  StringReader& operator=(const StringReader&) = delete;
 public:
  StringReader(std::string& Str) : Str(Str), Cursor(0) {}
  ~StringReader() {}

  size_t read(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool write(uint8_t* Buf, size_t Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;

 private:
  std::string Str;
  size_t Cursor;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif // DECOMPRESSOR_SRC_STREAM_STRINGREADER_H
