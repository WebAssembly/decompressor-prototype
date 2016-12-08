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

TraceClass::ContextPtr Writer::getTraceContext() {
  TraceClass::ContextPtr Ptr;
  return Ptr;
}

std::shared_ptr<TraceClassSexp> Writer::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>(getDefaultTraceName()));
  return Trace;
}

void Writer::setTrace(std::shared_ptr<filt::TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (Trace)
    Trace->addContext(getTraceContext());
}

const char* Writer::getDefaultTraceName() const {
  return "Writer";
}

void Writer::reset() {
}

bool Writer::writeMagicNumber(uint32_t MagicNumber) {
  return writeUint32(MagicNumber);
}

bool Writer::writeVersionNumber(uint32_t VersionNumber) {
  return writeUint32(VersionNumber);
}

bool Writer::writeFreezeEof() {
  return true;
}

void Writer::describeState(FILE* File) {
}

}  // end of namespace interp

}  // end of namespace wasm
