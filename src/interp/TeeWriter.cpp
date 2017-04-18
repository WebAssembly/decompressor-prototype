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

// Implements a tee that broadcasts write actions to a set of writers.

#include "interp/TeeWriter.h"

namespace wasm {

using namespace decode;
using namespace utils;

namespace interp {

TeeWriter::TeeWriter() : Writer(true) {}

TeeWriter::~TeeWriter() {}

void TeeWriter::add(std::shared_ptr<Writer> NodeWriter,
                    bool DefinesStreamType,
                    bool TraceNode,
                    bool TraceContext) {
  Writers.push_back(Node(NodeWriter, DefinesStreamType, TraceNode));
  if (TraceContext && !ContextWriter)
    ContextWriter = NodeWriter;
}

TeeWriter::Node::Node(std::shared_ptr<Writer> NodeWriter,
                      bool DefinesStreamType,
                      bool TraceNode)
    : NodeWriter(NodeWriter),
      TraceNode(TraceNode),
      DefinesStreamType(DefinesStreamType) {}

void TeeWriter::reset() {
  for (Node& Nd : Writers)
    Nd.getWriter().reset();
}

StreamType TeeWriter::getStreamType() const {
  bool IsDefined = false;
  StreamType Type = StreamType::Other;
  for (const Node& Nd : Writers) {
    if (Nd.getDefinesStreamType()) {
      if (!IsDefined) {
        IsDefined = true;
        Type = Nd.getWriter()->getStreamType();
        continue;
      }
      if (Type != Nd.getWriter()->getStreamType())
        Type = StreamType::Other;
    }
  }
  return Type;
}

bool TeeWriter::writeUint8(uint8_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeUint8(Value))
      return false;
  return true;
}

bool TeeWriter::writeUint32(uint32_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeUint32(Value))
      return false;
  return true;
}

bool TeeWriter::writeUint64(uint64_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeUint64(Value))
      return false;
  return true;
}

bool TeeWriter::writeVarint32(int32_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeVarint32(Value))
      return false;
  return true;
}

bool TeeWriter::writeVarint64(int64_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeVarint64(Value))
      return false;
  return true;
}

bool TeeWriter::writeVaruint32(uint32_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeVaruint32(Value))
      return false;
  return true;
}

bool TeeWriter::writeVaruint64(uint64_t Value) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeVaruint64(Value))
      return false;
  return true;
}

bool TeeWriter::alignToByte() {
  bool Result = true;
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->alignToByte())
      Result = false;
  return Result;
}

bool TeeWriter::writeBlockEnter() {
  bool Result = true;
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeBlockEnter())
      Result = false;
  return Result;
}

bool TeeWriter::writeBlockExit() {
  bool Result = true;
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeBlockExit())
      Result = false;
  return Result;
}

bool TeeWriter::writeFreezeEof() {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeFreezeEof())
      return false;
  return true;
}

bool TeeWriter::writeBinary(IntType Value, const filt::Node* Encoding) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeBinary(Value, Encoding))
      return false;
  return true;
}

bool TeeWriter::writeValue(decode::IntType Value, const filt::Node* Format) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeValue(Value, Format))
      return false;
  return true;
}

bool TeeWriter::writeTypedValue(decode::IntType Value,
                                interp::IntTypeFormat Format) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeTypedValue(Value, Format))
      return false;
  return true;
}

bool TeeWriter::writeHeaderValue(decode::IntType Value,
                                 interp::IntTypeFormat Format) {
  for (Node& Nd : Writers)
    if (!Nd.getWriter()->writeHeaderValue(Value, Format))
      return false;
  return true;
}

bool TeeWriter::writeAction(IntType Action) {
  for (Node& Nd : Writers) {
    if (!Nd.getWriter()->writeAction(Action))
      return false;
  }
  return true;
}

void TeeWriter::setMinimizeBlockSize(bool NewValue) {
  for (Node& Nd : Writers)
    Nd.getWriter()->setMinimizeBlockSize(NewValue);
}

void TeeWriter::describeState(FILE* File) {
  for (Node& Nd : Writers)
    Nd.getWriter()->describeState(File);
}

TraceContextPtr TeeWriter::getTraceContext() {
  if (ContextWriter)
    return ContextWriter->getTraceContext();
  return TraceContextPtr();
}

void TeeWriter::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  for (Node& Nd : Writers)
    if (Nd.getTraceNode())
      Nd.getWriter()->setTrace(NewTrace);
}

}  // end of namespace interp

}  // end of namespace wasm
