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

// Defines a tee that broadcasts write actions to a set of writers.
// Note: Write contexts and tracing is controllable for each
// individual writer in the tee.

#ifndef DECOMPRESSOR_SRC_INTERP_TEEWRITER_H
#define DECOMPRESSOR_SRC_INTERP_TEEWRITER_H

#include "interp/Writer.h"

#include <memory>
#include <vector>

namespace wasm {

namespace interp {

class TeeWriter : public Writer {
  TeeWriter(const TeeWriter&) = delete;
  TeeWriter& operator=(const TeeWriter&) = delete;

 public:
  class Node {
   public:
    Node(std::shared_ptr<Writer> NodeWriter,
         bool DefinesStreamType,
         bool TraceNode);

    std::shared_ptr<Writer> getWriter() { return NodeWriter; }
    const std::shared_ptr<Writer> getWriter() const { return NodeWriter; }
    bool getTraceNode() const { return TraceNode; }
    bool getDefinesStreamType() const { return DefinesStreamType; }

   private:
    std::shared_ptr<Writer> NodeWriter;
    bool TraceNode;
    bool DefinesStreamType;
  };

  TeeWriter();
  ~TeeWriter() OVERRIDE;

  // Warning: The first writer with TraceContext will be used as the
  // trace context of the TeeWriter.
  void add(std::shared_ptr<Writer> NodeWriter,
           bool DefinesStreamType,
           bool TraceNode,
           bool TraceContext);

  void reset() OVERRIDE;
  decode::StreamType getStreamType() const OVERRIDE;
  // Override the following as needed. These methods return false if the writes
  // failed. Default actions are to do nothing and return true.
  bool writeUint8(uint8_t Value) OVERRIDE;
  bool writeUint32(uint32_t Value) OVERRIDE;
  bool writeUint64(uint64_t Value) OVERRIDE;
  bool writeVarint32(int32_t Value) OVERRIDE;
  bool writeVarint64(int64_t Value) OVERRIDE;
  bool writeVaruint32(uint32_t Value) OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeBinary(decode::IntType, const filt::Node* Encoding) OVERRIDE;
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeTypedValue(decode::IntType Value,
                       interp::IntTypeFormat Format) OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeAction(const filt::SymbolNode* Action) OVERRIDE;

  void setMinimizeBlockSize(bool NewValue) OVERRIDE;
  void describeState(FILE* File) OVERRIDE;

  utils::TraceContextPtr getTraceContext() OVERRIDE;
  void setTrace(std::shared_ptr<utils::TraceClass> Trace) OVERRIDE;

 private:
  std::vector<Node> Writers;
  std::shared_ptr<Writer> ContextWriter;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  //  DECOMPRESSOR_SRC_INTERP_TEEWRITER_H
