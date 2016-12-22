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
    Node() = delete;

   public:
    Node(std::shared_ptr<Writer> NodeWriter,
         bool TraceNode,
         bool DefinesTraceContext,
         bool DefinesStreamType);

    std::shared_ptr<Writer> getWriter() { return NodeWriter; }
    const std::shared_ptr<Writer> getWriter() const { return NodeWriter; }
    bool getTraceNode() const { return TraceNode; }
    bool getDefinesTraceContext() const { return DefinesTraceContext; }
    bool getDefinesStreamType() const { return DefinesStreamType; }
    utils::TraceClass::ContextPtr getTraceContext();

   private:
    std::shared_ptr<Writer> NodeWriter;
    const bool TraceNode;
    const bool DefinesTraceContext;
    const bool DefinesStreamType;
    utils::TraceClass::ContextPtr TraceContext;
  };

  TeeWriter();
  ~TeeWriter() OVERRIDE;

  void add(std::shared_ptr<Writer> NodeWriter,
           bool DefinesStreamType = false,
           bool TraceNode = false,
           bool DefinesTraceContext = false);

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
  bool writeValue(decode::IntType Value, const filt::Node* Format) OVERRIDE;
  bool writeTypedValue(decode::IntType Value,
                       interp::IntTypeFormat Format) OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeAction(const filt::CallbackNode* Action) OVERRIDE;

  void setMinimizeBlockSize(bool NewValue) OVERRIDE;
  void describeState(FILE* File) OVERRIDE;

  utils::TraceClass::ContextPtr getTraceContext() OVERRIDE;

 private:
  class Context : public utils::TraceClass::Context {
    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

   public:
    TeeWriter* MyWriter;

    Context(TeeWriter* MyWriter) : MyWriter(MyWriter) {}
    ~Context() OVERRIDE;
    void describe(FILE* File) OVERRIDE;
  };

  std::vector<Node> Writers;
  std::shared_ptr<Context> MyContext;

  void describeContexts(FILE* File);
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  //  DECOMPRESSOR_SRC_INTERP_TEEWRITER_H
