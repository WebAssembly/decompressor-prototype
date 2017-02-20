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

#include "interp/FormatHelpers-templates.h"
#include "stream/ReadCursor.h"

namespace wasm {

namespace interp {

namespace fmt {

template uint8_t readUint8<decode::ReadCursor>(decode::ReadCursor&);

template uint32_t readUint32<decode::ReadCursor>(decode::ReadCursor&);

template uint64_t readUint64<decode::ReadCursor>(decode::ReadCursor&);

template int32_t readVarint32<decode::ReadCursor>(decode::ReadCursor&);

template int64_t readVarint64<decode::ReadCursor>(decode::ReadCursor&);

template uint32_t readVaruint32<decode::ReadCursor>(decode::ReadCursor&);

template uint64_t readVaruint64<decode::ReadCursor>(decode::ReadCursor&);

template uint32_t readFixed<uint32_t, decode::ReadCursor>(decode::ReadCursor&);

template uint32_t readLEB128Loop<uint32_t, decode::ReadCursor>(
    decode::ReadCursor&,
    uint32_t&,
    uint8_t&);

template uint32_t readLEB128<uint32_t, decode::ReadCursor>(decode::ReadCursor&);

template uint32_t readSignedLEB128<uint32_t, decode::ReadCursor>(
    decode::ReadCursor&);

template uint64_t readFixed<uint64_t, decode::ReadCursor>(decode::ReadCursor&);

template uint64_t readLEB128Loop<uint64_t, decode::ReadCursor>(
    decode::ReadCursor&,
    uint32_t&,
    uint8_t&);

template uint64_t readLEB128<uint64_t, decode::ReadCursor>(decode::ReadCursor&);

template uint64_t readSignedLEB128<uint64_t, decode::ReadCursor>(
    decode::ReadCursor&);

}  // end of namespace fmt

}  // end of namespace decode

}  // end of namespace wasm
