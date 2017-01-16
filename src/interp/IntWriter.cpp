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
using namespace utils;

namespace interp {

IntWriter::IntWriter(std::shared_ptr<IntStream> Output)
    : Output(Output), Pos(Output) {
}

void IntWriter::reset() {
  Output->reset();
  IntStream::WriteCursorWithTraceContext StartPos(Output);
  Pos = StartPos;
}

TraceClass::ContextPtr IntWriter::getTraceContext() {
  return Pos.getTraceContext();
}

const char* IntWriter::getDefaultTraceName() const {
  return "IntWriter";
}

StreamType IntWriter::getStreamType() const {
  return StreamType::Int;
}

bool IntWriter::writeUint8(uint8_t Value) {
  return write(Value);
}

bool IntWriter::writeUint32(uint32_t Value) {
  return write(Value);
}

bool IntWriter::writeUint64(uint64_t Value) {
  return write(Value);
}

bool IntWriter::writeVarint32(int32_t Value) {
  return write(Value);
}

bool IntWriter::writeVarint64(int64_t Value) {
  return write(Value);
}

bool IntWriter::writeVaruint32(uint32_t Value) {
  return write(Value);
}

bool IntWriter::writeVaruint64(uint64_t Value) {
  return write(Value);
}

bool IntWriter::writeFreezeEof() {
  return Pos.freezeEof();
}

bool IntWriter::writeHeaderValue(IntType Value, IntTypeFormat Format) {
  Output->appendHeader(Value, Format);
  return true;
}

bool IntWriter::writeAction(const filt::SymbolNode* Action) {
  switch (Action->getPredefinedSymbol()) {
    case PredefinedSymbol::Block_enter:
    case PredefinedSymbol::Block_enter_writeonly:
      return Pos.openBlock();
    case PredefinedSymbol::Block_exit:
    case PredefinedSymbol::Block_exit_writeonly:
      return Pos.closeBlock();
    default:
      // Ignore all other actions, since they do not involve
      // changing the integer sequence.
      return true;
  }
}

void IntWriter::describeState(FILE* File) {
  fprintf(stderr, "Pos = ");
  Pos.describe(File);
  fputc('\n', File);
}

}  // end of namespace interp

}  // end of namespace wasm
