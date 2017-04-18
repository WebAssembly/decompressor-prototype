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
///

#include "stream/BlockEob.h"

namespace wasm {

namespace decode {

BlockEob::BlockEob(AddressType Address) : EobAddress(Address) {
  init();
}

BlockEob::BlockEob(AddressType ByteAddr,
                   const std::shared_ptr<BlockEob> EnclosingEobPtr)
    : EobAddress(ByteAddr), EnclosingEobPtr(EnclosingEobPtr) {
  init();
}

BlockEob::~BlockEob() {}

void BlockEob::fail() {
  BlockEob* Next = this;
  while (Next) {
    resetAddress(Next->EobAddress);
    Next = Next->EnclosingEobPtr.get();
  }
}

FILE* BlockEob::describe(FILE* File) const {
  fprintf(File, "eob=");
  describeAddress(File, EobAddress);
  return File;
}

}  // end of decode namespace

}  // end of wasm namespace
