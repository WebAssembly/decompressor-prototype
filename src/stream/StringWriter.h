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

#ifndef DECOMPRESSOR_SRC_STREAM_STRINGWRITER_H_
#define DECOMPRESSOR_SRC_STREAM_STRINGWRITER_H_

#include <string>

#include "stream/RawStream.h"

namespace wasm {

namespace decode {

class StringWriter : public RawStream {
  StringWriter() = delete;
  StringWriter(const StringWriter&) = delete;
  StringWriter& operator=(const StringWriter&) = delete;

 public:
  StringWriter(std::string& Str);
  ~StringWriter();

  AddressType read(ByteType* Buf, AddressType Size = 1) OVERRIDE;
  bool write(ByteType* Buf, AddressType Size = 1) OVERRIDE;
  bool freeze() OVERRIDE;
  bool atEof() OVERRIDE;

 private:
  std::string& Str;
  bool IsFrozen;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_STRINGWRITER_H_
