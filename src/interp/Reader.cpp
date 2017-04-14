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

// implements a reader for wasm/casm files.

#include "interp/Reader.h"

#include "sexp/Ast.h"
#include "utils/Trace.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

TraceContextPtr Reader::getTraceContext() {
  TraceContextPtr Ptr;
  return Ptr;
}

void Reader::setTraceProgress(bool NewValue) {
  if (!NewValue && !Trace)
    return;
  getTrace().setTraceProgress(NewValue);
}

void Reader::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
  if (Trace) {
    Trace->addContext(getTraceContext());
  }
}

const char* Reader::getDefaultTraceName() const {
  return "Reader";
}

std::shared_ptr<TraceClass> Reader::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>(getDefaultTraceName()));
  return Trace;
}

void Reader::reset() {
}

bool Reader::alignToByte() {
  return true;
}

bool Reader::readBlockEnter() {
  return true;
}

bool Reader::readBlockExit() {
  return true;
}

uint8_t Reader::readBit() {
  return readVaruint64() & 0x1;
}

uint8_t Reader::readUint8() {
  return uint8_t(readVaruint64());
}

uint32_t Reader::readUint32() {
  return uint32_t(readVaruint64());
}

uint64_t Reader::readUint64() {
  return uint64_t(readVaruint64());
}

int32_t Reader::readVarint32() {
  return int32_t(readVaruint64());
}

int64_t Reader::readVarint64() {
  return int64_t(readVaruint64());
}

uint32_t Reader::readVaruint32() {
  return uint32_t(readVaruint64());
}

bool Reader::readBinary(const Node*, IntType& Value) {
  Value = readVaruint64();
  return true;
}

bool Reader::readValue(const filt::Node* Format, IntType& Value) {
  switch (Format->getType()) {
    case NodeType::Bit:
      Value = readBit();
      return true;
    case NodeType::Uint8:
      Value = readUint8();
      return true;
    case NodeType::Uint32:
      Value = readUint32();
      return true;
    case NodeType::Uint64:
      Value = readUint64();
      return true;
    case NodeType::Varint32:
      Value = readVarint32();
      return true;
    case NodeType::Varint64:
      Value = readVarint64();
      return true;
    case NodeType::Varuint32:
      Value = readVaruint32();
      return true;
    case NodeType::Varuint64:
      Value = readVaruint64();
      return true;
    default:
      Value = 0;
      return false;
  }
}

bool Reader::readHeaderValue(IntTypeFormat Format, IntType& Value) {
  switch (Format) {
    case IntTypeFormat::Uint8:
      Value = readUint8();
      return true;
    case IntTypeFormat::Uint32:
      Value = readUint32();
      return true;
    case IntTypeFormat::Uint64:
      Value = readUint64();
      return true;
    default:
      Value = 0;
      return false;
  }
}

bool Reader::readAction(IntType Action) {
  switch (Action) {
    case IntType(PredefinedSymbol::Block_enter):
    case IntType(PredefinedSymbol::Block_enter_readonly):
      return readBlockEnter();
    case IntType(PredefinedSymbol::Block_exit):
    case IntType(PredefinedSymbol::Block_exit_readonly):
      return readBlockExit();
    case IntType(PredefinedSymbol::Align):
      return alignToByte();
    default:
      return DefaultReadAction;
  }
}

bool Reader::tablePush(IntType Value) {
  return true;
}

bool Reader::tablePop() {
  return true;
}

}  // end of namespace interp

}  // end of namespace wasm
