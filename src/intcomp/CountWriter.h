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

// Defines a writer that counts usage patterns within the written values.
// This includes blocks as well as sequences of integers.

#ifndef DECOMPRESSOR_SRC_INTCOMP_COUNTWRITER_H
#define DECOMPRESSOR_SRC_INTCOMP_COUNTWRITER_H

#include "intcomp/IntCountNode.h"
#include "interp/Writer.h"

#include <set>
#include <vector>

namespace wasm {

namespace intcomp {

// Note: This class assumes that the input (integer sequence) is
// processed twice.  The first time, "UpToSize" is set to 1 to capture
// the frequency usage of each integer in the input. The second time,
// "UpToSize" defines the maximumal sequence of integers it should
// collect on.
class CountWriter : public interp::Writer {
  CountWriter() = delete;
  CountWriter(const CountWriter&) = delete;
  CountWriter& operator=(const CountWriter&) = delete;

 public:
  typedef std::vector<CountNode::IntPtr> IntFrontier;
  typedef std::set<CountNode::IntPtr> CountNodeIntSet;
  CountWriter(CountNode::RootPtr Root)
      : Root(Root), CountCutoff(1), UpToSize(0) {}

  ~CountWriter() OVERRIDE;

  void setCountCutoff(uint64_t NewValue) { CountCutoff = NewValue; }
  void setUpToSize(size_t NewSize) {
    assert(NewSize >= 1);
    UpToSize = NewSize;
  }
  void resetUpToSize() { UpToSize = 0; }
  size_t getUpToSize() const { return UpToSize; }

  void addToUsageMap(decode::IntType Value);

  decode::StreamType getStreamType() const OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeAction(const filt::SymbolNode* Action) OVERRIDE;

 private:
  CountNode::RootPtr Root;
  IntFrontier Frontier;
  uint64_t CountCutoff;
  size_t UpToSize;
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_COUNTWRITER_H
