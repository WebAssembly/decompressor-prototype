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

#ifndef DECOMPRESSOR_SRC_BINARY_BINARYCODES_H
#define DECOMPRESSOR_SRC_BINARY_BINARYCODES_H

#include "binary/BinaryCodes.def"

namespace wasm {

namespace filt {

enum class SectionCode : uint32_t {
#define X(tag, code) tag = code,
  WASM_SECTION_CODES_TABLE
#undef X
};

}  // end of namespace filt

}  // end of namespace wasm

#endif // DECOMPRESSOR_SRC_BINARY_BINARYCODES_H
