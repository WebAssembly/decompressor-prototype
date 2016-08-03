/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Defines a byte queue to buffer bit and byte streams.

#ifndef DECOMPRESSOR_SRC_STREAM_BYTEQUEUE_H
#define DECOMPRESSOR_SRC_STREAM_BYTEQUEUE_H

#include "stream/Queue.h"
#include "stream/RawStream.h"

namespace wasm {

namespace decode {

class ByteQueue : public Queue<uint8_t> {
  ByteQueue(const ByteQueue&) = delete;
  ByteQueue& operator=(const ByteQueue&) = delete;

 public:
  ByteQueue() {}

  ~ByteQueue() OVERRIDE {}

  // TODO(karlschimpf): Rename the following to not conflict read/write of
  // inherited class Queue<uint8_t>.

  // Reads a contiguous range of bytes into a buffer.
  //
  // Note: A read request may not be fully met. This function only guarantees
  // to read 1 element from the queue, if eob hasn't been reached. This is
  // done to minimize blocking. When possible, it will try to meet the full
  // request.
  //

  // @param Address The address within the queue to read from. Automatically
  //                incremented during read.
  // @param Buffer  A pointer to a buffer to be filled (and contains at least
  //                Size elements).
  // @param Size    The number of requested elements to read.
  // @result        The actual number of elements read. If zero, the eob was
  //                reached. Valid return values are in [0..Size].
  size_t read(size_t& Address, uint8_t* Buffer, size_t Size = 1);

  // Writes a contiquous sequence of bytes in the given buffer.
  //
  // Note: If Address is larger than queue size, zero's are automatically
  // inserted.
  //
  // @param Address The address within the queue to write to. Automatically
  //                incremented during write.
  // @param Buffer  A pointer to the buffer of elements to write.
  // @param Size    The number of elements in the buffer to write.
  // @result        True if successful (i.e. not beyond eob address).
  bool write(size_t& Address, uint8_t* Buffer, size_t Size = 1);

  // For debugging. Writes out sequence of bytes (on page associated with
  // Address) in the queue.
  void writePageAt(FILE* File, size_t Address);
};

// Read-only queue that is write-filled from a steam using the given
// Reader.
class ReadBackedByteQueue FINAL : public ByteQueue {
  ReadBackedByteQueue(const ReadBackedByteQueue&) = delete;
  ReadBackedByteQueue& operator=(const ReadBackedByteQueue&) = delete;
  ReadBackedByteQueue() = delete;

 public:
  ReadBackedByteQueue(std::unique_ptr<RawStream> _Reader) {
    assert(_Reader);
    Reader = std::move(_Reader);
  }
  ~ReadBackedByteQueue() OVERRIDE {}

 private:
  // Reader to write fill buffer as needed.
  std::unique_ptr<RawStream> Reader;

  bool readFill(size_t Address) OVERRIDE;
};

// Write-only queue that is dumped to a stream using the given Writer.
class WriteBackedByteQueue FINAL : public ByteQueue {
  WriteBackedByteQueue(const WriteBackedByteQueue&) = delete;
  WriteBackedByteQueue& operator=(const WriteBackedByteQueue&) = delete;
  WriteBackedByteQueue() = delete;

 public:
  WriteBackedByteQueue(std::unique_ptr<RawStream> _Writer) {
    assert(_Writer);
    Writer = std::move(_Writer);
  }
  ~WriteBackedByteQueue();

 private:
  // Writer to dump contents of queue, when the contents is no longer
  // needed by reader.
  std::unique_ptr<RawStream> Writer;

  void dumpFirstPage() OVERRIDE;
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_BYTEQUEUE_H
