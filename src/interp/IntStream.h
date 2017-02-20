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

// Defines a (non-file based) integer stream.

#ifndef DECOMPRESSOR_SRC_INTERP_INTSTREAM_H_
#define DECOMPRESSOR_SRC_INTERP_INTSTREAM_H_

#include <vector>

#include "interp/IntFormats.h"
#include "utils/TraceAPI.h"

namespace wasm {

namespace interp {

class IntStream : public std::enable_shared_from_this<IntStream> {
  IntStream(const IntStream&) = delete;
  IntStream& operator=(const IntStream&) = delete;

 public:
  class Block;
  class Cursor;
  class WriteCursor;
  typedef std::vector<decode::IntType> IntVector;
  typedef std::vector<std::pair<decode::IntType, IntTypeFormat>> HeaderVector;
  typedef std::shared_ptr<Block> BlockPtr;
  typedef std::vector<BlockPtr> BlockVector;
  typedef BlockVector::iterator BlockIterator;
  typedef std::shared_ptr<IntStream> StreamPtr;
  class Block : public std::enable_shared_from_this<Block> {
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    friend class WriteCursor;

   public:
    explicit Block(size_t BeginIndex = 0,
                   size_t EndIndex = std::numeric_limits<size_t>::max());
    ~Block();
    size_t getBeginIndex() const { return BeginIndex; }
    size_t getEndIndex() const { return EndIndex; }

    void describe(FILE* File);

   private:
    size_t BeginIndex;
    size_t EndIndex;
    BlockVector Subblocks;
  };

  class Cursor : public std::enable_shared_from_this<Cursor> {
   public:
    class TraceContext;

    Cursor();
    explicit Cursor(StreamPtr Stream);
    explicit Cursor(const Cursor& C);
    Cursor& operator=(const Cursor& C);
    ~Cursor();
    size_t getIndex() const { return Index; }
    size_t streamSize() const { return Stream->size(); }
    bool atEof() const;
    bool atEob() const;
    bool atEnd() const;

    // For debugging.
    FILE* describe(FILE* File,
                   bool IncludeDetail = false,
                   bool AddEoln = false);
    utils::TraceContextPtr getTraceContext();

   protected:
    size_t Index;
    BlockVector EnclosingBlocks;
    StreamPtr Stream;
    BlockPtr closeBlock();
  };

  class WriteCursor : public Cursor {
   public:
    WriteCursor();
    explicit WriteCursor(StreamPtr Stream);
    explicit WriteCursor(const Cursor& C);
    ~WriteCursor();
    WriteCursor& operator=(const WriteCursor& C) {
      Cursor::operator=(C);
      return *this;
    }
    bool write(decode::IntType Value);
    bool freezeEof();
    bool openBlock();
    bool closeBlock();
  };

  class ReadCursor : public Cursor {
   public:
    ReadCursor();
    explicit ReadCursor(StreamPtr Stream);
    explicit ReadCursor(const ReadCursor& C);
    ~ReadCursor();
    ReadCursor& operator=(const ReadCursor& C) {
      Cursor::operator=(C);
      NextBlock = C.NextBlock;
      EndBlocks = C.EndBlocks;
      return *this;
    }
    decode::IntType read();
    bool openBlock();
    bool closeBlock();
    bool hasMoreBlocks() const { return NextBlock != EndBlocks; }
    BlockPtr getNextBlock() const { return *NextBlock; }

   private:
    IntStream::BlockIterator NextBlock;
    IntStream::BlockIterator EndBlocks;
  };

  // WARNING: Don't call constructor directly. Call std::make_shared().
  IntStream();
  void reset();
  ~IntStream();

  size_t size() const { return Values.size(); }
  size_t getNumIntegers() const;
  BlockPtr getTopBlock() { return TopBlock; }
  bool isFrozen() const { return isFrozenFlag; }

  BlockIterator getBlocksBegin();
  BlockIterator getBlocksEnd();

  void describe(FILE* File, const char* Name = nullptr);

  const HeaderVector& getHeader() { return Header; }
  void appendHeader(decode::IntType Value, interp::IntTypeFormat Format);

 private:
  HeaderVector Header;
  IntVector Values;
  BlockPtr TopBlock;
  bool isFrozenFlag;

  // The following fields is defined by openBlock(), and defines the
  // sequence of written blocks.
  BlockVector Blocks;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTSTREAM_H_
