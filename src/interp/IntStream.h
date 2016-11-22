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

#ifndef DECOMPRESSOR_SRC_INTERP_INTSTREAM_H
#define DECOMPRESSOR_SRC_INTERP_INTSTREAM_H

#include "utils/Defs.h"

#include <map>
#include <memory>
#include <vector>

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
  typedef std::shared_ptr<Block> BlockPtr;
  typedef std::vector<BlockPtr> BlockVector;
  typedef BlockVector::iterator BlockIterator;
  typedef std::shared_ptr<IntStream> StreamPtr;
  class Block : public std::enable_shared_from_this<Block> {
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    friend class WriteCursor;

   public:
    // WARNING: Don't call constructor directly. Call std::make_shared().
    explicit Block(size_t BeginIndex = 0,
                   size_t EndIndex = std::numeric_limits<size_t>::max())
        : BeginIndex(BeginIndex), EndIndex(EndIndex) {}
    size_t getBeginIndex() const { return BeginIndex; }
    size_t getEndIndex() const { return EndIndex; }

   private:
    size_t BeginIndex;
    size_t EndIndex;
    BlockVector Subblocks;
  };

  class Cursor {
   public:
    // Warning: Don't call constructor directly. Call std::make_shared().
    Cursor() : Index(0) {}
    explicit Cursor(StreamPtr Stream);
    explicit Cursor(const Cursor& C);
    Cursor& operator=(const Cursor& C);
    ~Cursor() {}
    size_t getIndex() const { return Index; }
    size_t sizeAvailable() const { return Stream->size() - Index; }
    bool atEof() const;
    bool atEob() const;
    bool atEnd() const;

   protected:
    size_t Index;
    BlockVector EnclosingBlocks;
    StreamPtr Stream;
    BlockPtr closeBlock();
  };

  class WriteCursor : public Cursor {
   public:
    WriteCursor() : Cursor() {}
    explicit WriteCursor(StreamPtr Stream) : Cursor(Stream) {}
    explicit WriteCursor(const Cursor& C) : Cursor(C) {}
    bool write(decode::IntType Value);
    bool freezeEof();
    bool openBlock();
    bool closeBlock();
  };

  class ReadCursor : public Cursor {
   public:
    ReadCursor() : Cursor() {}
    explicit ReadCursor(StreamPtr Stream)
        : Cursor(Stream),
          NextBlock(Stream->getBlocksBegin()),
          EndBlocks(Stream->getBlocksEnd()) {}
    explicit ReadCursor(const ReadCursor& C)
        : Cursor(C), NextBlock(C.NextBlock), EndBlocks(C.EndBlocks) {}
    ReadCursor& operator=(const ReadCursor& C) {
      Cursor::operator=(C);
      NextBlock = C.NextBlock;
      EndBlocks = C.EndBlocks;
      return *this;
    }
    decode::IntType read();
    bool openBlock();
    bool closeBlock();

   private:
    IntStream::BlockIterator NextBlock;
    IntStream::BlockIterator EndBlocks;
  };

  // WARNING: Don't call constructor directly. Call std::make_shared().
  IntStream() : TopBlock(std::make_shared<Block>()), isFrozenFlag(false) {}
  void reset() { TopBlock.reset(); }
  ~IntStream() {}

  size_t size() const { return Values.size(); }
  BlockPtr getTopBlock() { return TopBlock; }
  bool isFrozen() const { return isFrozenFlag; }

  BlockIterator getBlocksBegin() { return Blocks.begin(); }
  BlockIterator getBlocksEnd() { return Blocks.end(); }

 private:
  IntVector Values;
  BlockPtr TopBlock;
  bool isFrozenFlag;

  // The following fields is defined by openWriteBlock(), and defines the
  // sequence of written blocks.
  BlockVector Blocks;
};

}  // end of namespace interp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_INTSTREAM_H
