// -*- C++ -*-
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

// Defines a pointer into a byte stream.

#ifndef DECOMPRESSOR_SRC_STREAM_CURSOR_H
#define DECOMPRESSOR_SRC_STREAM_CURSOR_H

#include "stream/PageCursor.h"
#include "utils/Trace.h"

namespace wasm {

namespace decode {

class BlockEob;
class Queue;

class Cursor : public PageCursor {
  Cursor& operator=(const Cursor&) = delete;

 public:
  class TraceContext : public utils::TraceContext {
    TraceContext() = delete;
    TraceContext(const TraceContext&) = delete;
    TraceContext& operator=(const TraceContext&) = delete;

   public:
    TraceContext(Cursor& Pos);
    ~TraceContext() OVERRIDE;
    void describe(FILE* File) OVERRIDE;

   private:
    Cursor& Pos;
  };
  virtual ~Cursor();
  void swap(Cursor& C);
  void assign(const Cursor& C);
  StreamType getType() const { return Type; }
  bool isQueueGood() const;
  bool isBroken() const;
  std::shared_ptr<Queue> getQueue();
  bool isEofFrozen() const;
  virtual bool atEof() const;
  AddressType getEofAddress() const;
  AddressType& getEobAddress() const;
  void freezeEof();
  void close();
  AddressType fillSize();
  AddressType getAddress() const { return CurAddress; }

  // For debugging.
  FILE* describe(FILE* File, bool IncludeDetail = false, bool AddEoln = false);
  // Adds any extentions to the page address, as defined in a derived class.
  virtual void describeDerivedExtensions(FILE* File, bool IncludeDetail);
  virtual utils::TraceContextPtr getTraceContext();

 protected:
  StreamType Type;
  // The byte queue the cursor points to.
  std::shared_ptr<Queue> Que;
  // End of block address.
  std::shared_ptr<BlockEob> EobPtr;
  ByteType CurByte;
  AddressType GuaranteedBeforeEob;

  Cursor(StreamType Type, std::shared_ptr<Queue> Que);
  explicit Cursor(const Cursor& C);
  Cursor(const Cursor& C, AddressType StartAddress, bool ForRead);

  // Note: The nullary cursor should not be used until it has been assigned
  // a valid value.
  Cursor();

  void updateGuaranteedBeforeEob();

  // Returns true if able to fill the buffer with at least one byte.
  bool readFillBuffer();

  // Creates new pages in buffer so that writes can occur. WantedSize is
  // a hint of the expecte growth.
  void writeFillBuffer(AddressType WantedSize = PageSize);

  void fail();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_CURSOR_H
