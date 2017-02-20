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
#include "stream/WriteCursor.h"

namespace wasm {

namespace interp {

namespace fmt {

template void writeUint8<decode::WriteCursor>(uint8_t, decode::WriteCursor&);

template void writeUint32<decode::WriteCursor>(uint32_t, decode::WriteCursor&);

template void writeUint64<decode::WriteCursor>(uint64_t, decode::WriteCursor&);

template void writeVarint32<decode::WriteCursor>(int32_t, decode::WriteCursor&);

template void writeVarint64<decode::WriteCursor>(int64_t, decode::WriteCursor&);

template void writeVaruint32<decode::WriteCursor>(uint32_t,
                                                  decode::WriteCursor&);

template void writeVaruint64<decode::WriteCursor>(uint64_t,
                                                  decode::WriteCursor&);

template void writeFixedVaruint32<decode::WriteCursor>(uint32_t,
                                                       decode::WriteCursor&);

template void writeLEB128<uint32_t, decode::WriteCursor>(uint32_t,
                                                         decode::WriteCursor&);

// TODO - factor out int32_t
template void writePositiveLEB128<int32_t, decode::WriteCursor>(
    int32_t,
    decode::WriteCursor&);

template void writeNegativeLEB128<int32_t, decode::WriteCursor>(
    int32_t Value,
    decode::WriteCursor& Pos);

template void writeFixedLEB128<uint32_t, decode::WriteCursor>(
    uint32_t,
    decode::WriteCursor&);

template void writeFixed<uint32_t, decode::WriteCursor>(uint32_t,
                                                        decode::WriteCursor&);

template void writeLEB128<uint64_t, decode::WriteCursor>(uint64_t,
                                                         decode::WriteCursor&);

template void writePositiveLEB128<uint64_t, decode::WriteCursor>(
    uint64_t,
    decode::WriteCursor&);

// TODO - factor out int64_t
template void writeNegativeLEB128<int64_t, decode::WriteCursor>(
    int64_t,
    decode::WriteCursor&);

template void writeFixedLEB128<int64_t, decode::WriteCursor>(
    int64_t,
    decode::WriteCursor&);

template void writeFixed<uint64_t, decode::WriteCursor>(uint64_t,
                                                        decode::WriteCursor&);

}  // end of namespace fmt

}  // end of namespace decode

}  // end of namespace wasm
