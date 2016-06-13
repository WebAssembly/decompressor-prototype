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

/* Defines interface for reading/writing to persistant (raw) streams. */

#ifndef DECOMPRESSOR_SRC_STREAM_RAWSTREAM_H
#define DECOMPRESSOR_SRC_STREAM_RAWSTREAM_H

#include "Defs.h"

namespace wasm {

namespace decode {

class RawStream {
  RawStream(const RawStream&) = delete;
  RawStream &operator=(const RawStream&) = delete;
public:
  RawStream() {}
  virtual ~RawStream() = default;

  // Reads a contiguous range of elements into a buffer.
  //
  // @param Buf     - A pointer to a buffer to be filled.
  // @param BufSize - The size of the buffer to read into.
  // @result        - The number of bytes read, or 0 if unable
  //                  to read anymore bytes.
  virtual size_t read(uint8_t *Buf, size_t Size = 1) = 0;


  // Writes a contigues range of elements from a buffer.
  //
  // @param Buf     - A pointer to a buffer to write from.
  // @param BufSize - The size of the buffer to write from.
  // @result        - True if successful.
  virtual bool write(uint8_t *Buf, size_t Size = 1) = 0;

  // Communicates that the stream can no longer be modified. Returns
  // false if unable to freeze.
  virtual bool freeze() = 0;

  // Returns true if at the end of stream.
  virtual bool atEof() = 0;
};

} // end of namespace decode

} // end of namespace decode

#endif // DECOMPRESSOR_SRC_STREAM_RAWSTREAM_H
