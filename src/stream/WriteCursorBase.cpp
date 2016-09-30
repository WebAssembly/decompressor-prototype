// -*- C++ -*-
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

#include "stream/WriteCursorBase.h"

namespace wasm {

namespace decode {

void WriteCursorBase::writeBits(uint32_t Value, uint32_t NumBits) {
  assert(NumBits <= sizeof(uint32_t) * CHAR_BIT);
  while (NumBits > 0) {
    const BitsInByteType AvailBits = CurByte.getWriteBitsRemaining();
    if (AvailBits >= NumBits) {
      CurByte.writeBits(Value, NumBits);
      if (AvailBits == NumBits) {
        writeByte(CurByte.getValue());
        CurByte.reset();
      }
      return;
    }
    uint32_t Shift = NumBits - AvailBits;
    CurByte.writeBits(Value >> Shift, AvailBits);
    Value &= (uint32_t(1) << Shift) - 1;
    NumBits -= AvailBits;
    writeByte(CurByte.getValue());
    CurByte.reset();
  }
}

}  // end of namespace decode

}  // end of namespace wasm
