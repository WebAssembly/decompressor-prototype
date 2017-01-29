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

// implements a writer for wasm/casm files.

#include "interp/Writer.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

Writer::~Writer() {
}

void Writer::setMinimizeBlockSize(bool NewValue) {
  MinimizeBlockSize = NewValue;
}

TraceClass::ContextPtr Writer::getTraceContext() {
  TraceClass::ContextPtr Ptr;
  return Ptr;
}

std::shared_ptr<TraceClass> Writer::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>(getDefaultTraceName()));
  return Trace;
}

void Writer::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (Trace)
    Trace->addContext(getTraceContext());
}

const char* Writer::getDefaultTraceName() const {
  return "Writer";
}

void Writer::reset() {
}

bool Writer::writeFreezeEof() {
  return true;
}

bool Writer::writeBinary(IntType Value, const Node* Format) {
  return writeVaruint64(Value);
}

bool Writer::writeTypedValue(IntType Value, IntTypeFormat Format) {
  switch (Format) {
    case IntTypeFormat::Uint8:
      return writeUint8(Value);
    case IntTypeFormat::Uint32:
      return writeUint32(Value);
    case IntTypeFormat::Uint64:
      return writeUint64(Value);
    case IntTypeFormat::Varint32:
      return writeVarint32(Value);
    case IntTypeFormat::Varint64:
      return writeVarint64(Value);
    case IntTypeFormat::Varuint32:
      return writeVaruint32(Value);
    case IntTypeFormat::Varuint64:
      return writeVaruint64(Value);
  }
  WASM_RETURN_UNREACHABLE(false);
}

bool Writer::writeValue(decode::IntType Value, const filt::Node* Format) {
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
      return true;
    default:
      return false;
  }
}

bool Writer::writeHeaderValue(IntType Value, IntTypeFormat Format) {
  return writeTypedValue(Value, Format);
}

void Writer::describeState(FILE* File) {
}

}  // end of namespace interp

}  // end of namespace wasm
