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

// Implements possible formats (and the number of bytes needed) for
// Implementseach integer value.

#include "intcomp/IntFormats.h"

namespace wasm {

namespace intcomp {

namespace {

const char* IntTypeFormatName[NumIntTypeFormats] = {
  "uint8",
  "uint32",
  "uint64",
  "varint32",
  "varint64",
  "varuint32",
  "varuint64"
};

} // end of anonymous namespace

const char* getName(IntTypeFormat Fmt) {
  return IntTypeFormatName[size_t(Fmt)];
}

}  // end of namespace intcomp

}  // end of namespace wasm
