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

// Defines helper methods for WASM formats.

#ifndef DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H_
#define DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H_

#include "utils/Defs.h"

namespace wasm {

namespace interp {

namespace fmt {

template <class Type, class ReadCursor>
Type readFixed(ReadCursor& Pos);

template <class Type, class ReadCursor>
Type readLEB128Loop(ReadCursor& Pos, uint32_t& Shift, decode::ByteType& Chunk);

template <class Type, class ReadCursor>
Type readLEB128(ReadCursor& Pos);

template <class Type, class ReadCursor>
Type readSignedLEB128(ReadCursor& Pos);

template <class ReadCursor>
uint8_t readUint8(ReadCursor& Pos);

template <class ReadCursor>
uint32_t readUint32(ReadCursor& Pos);

template <class ReadCursor>
uint64_t readUint64(ReadCursor& Pos);

template <class ReadCursor>
int32_t readVarint32(ReadCursor& Pos);

template <class ReadCursor>
int64_t readVarint64(ReadCursor& Pos);

template <class ReadCursor>
uint32_t readVaruint32(ReadCursor& Pos);

template <class ReadCursor>
uint64_t readVaruint64(ReadCursor& Pos);

template <class Type, class WriteCursor>
void writeLEB128(Type Value, WriteCursor& Pos);

template <class Type, class WriteCursor>
void writePositiveLEB128(Type Value, WriteCursor& Pos);

template <class Type, class WriteCursor>
void writeNegativeLEB128(Type Value, WriteCursor& Pos);

template <class Type, class WriteCursor>
void writeFixedLEB128(Type Value, WriteCursor& Pos);

template <class Type, class WriteCursor>
void writeFixed(Type Value, WriteCursor& Pos);

template <class WriteCursor>
void writeUint8(uint8_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeUint32(uint32_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeUint64(uint64_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeVarint32(int32_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeVarint64(int64_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeVaruint32(uint32_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeVaruint64(uint64_t Value, WriteCursor& Pos);

template <class WriteCursor>
void writeFixedVaruint32(uint32_t Value, WriteCursor& Pos);

}  // end of namespace fmt

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTERP_FORMATHELPERS_H_
