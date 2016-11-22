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

// Implements a writer for a (non-file based) integer stream.

#include "interp/IntWriter.h"

namespace wasm {

using namespace decode;
using namespace filt;

namespace interp {

void IntWriter::reset() {
  Output.reset();
}

StreamType IntWriter::getStreamType() const {
  return StreamType::Int;
}

bool IntWriter::writeUint8(uint8_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeUint32(uint32_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeUint64(uint64_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeVarint32(int32_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeVarint64(int64_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeVaruint32(uint32_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeVaruint64(uint64_t Value) {
  return Pos.write(Value);
}

bool IntWriter::writeFreezeEof() {
  return Pos.freezeEof();
}

bool IntWriter::writeValue(decode::IntType Value, const filt::Node* Format) {
  // Note: We pass through virtual functions to force any applicable cast
  // conversions.
  switch (Format->getType()) {
    case OpUint8:
      writeUint8(Value);
      return true;
    case OpUint32:
      writeUint32(Value);
      return true;
    case OpUint64:
      writeUint64(Value);
      return true;
    case OpVarint32:
      writeVarint32(Value);
      return true;
    case OpVarint64:
      writeVarint64(Value);
      return true;
    case OpVaruint32:
      writeVaruint32(Value);
      return true;
    case OpVaruint64:
      writeVaruint64(Value);
    default:
      return false;
  }
}

bool IntWriter::writeAction(const filt::CallbackNode* Action) {
  const auto* Sym = dyn_cast<SymbolNode>(Action->getKid(0));
  if (Sym == nullptr)
    return false;
  switch (Sym->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
      return Pos.openBlock();
      return true;
    case PredefinedSymbol::Block_exit:
      return Pos.closeBlock();
    default:
      return false;
  }
}

}  // end of namespace interp

}  // end of namespace wasm
