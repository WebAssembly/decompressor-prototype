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

// Defines the compressor of WASM files based on integer usage.

#ifndef DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H
#define DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H

#include "intcomp/IntCountNode.h"
#include "interp/Reader.h"
#include "interp/TraceSexpReader.h"
#include "interp/StreamWriter.h"

namespace wasm {

namespace intcomp {

class IntCompressor FINAL {
  IntCompressor() = delete;
  IntCompressor(const IntCompressor&) = delete;
  IntCompressor& operator=(const IntCompressor&) = delete;

 public:
  IntCompressor(std::shared_ptr<decode::Queue> InputStream,
                std::shared_ptr<decode::Queue> OutputStream,
                std::shared_ptr<filt::SymbolTable> Symtab);

  ~IntCompressor();

  bool errorsFound() const {
    return Input == nullptr && Input->errorsFound();
  }

  void compress();

  void setTraceProgress(bool NewValue) {
    if (Trace)
      Trace->setTraceProgress(NewValue);
  }

  void setMinimizeBlockSize(bool NewValue) { (void)NewValue; }

  interp::TraceClassSexpReader& getTrace() { return *Trace; }

 private:
  std::shared_ptr<filt::SymbolTable> Symtab;
  interp::Reader *Input;
  interp::Writer *Output;
  IntCountUsageMap UsageMap;
  interp::TraceClassSexpReader* Trace;

  void describe(FILE* Out);
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_INTCOMPRESS_H
