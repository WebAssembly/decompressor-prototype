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

// Defines a std::vector defined using an allocator.

#ifndef DECOMPRESSOR_SRC_ADT_ARENA_VECTOR_H
#define DECOMPRESSOR_SRC_ADT_ARENA_VECTOR_H

#include "utils/Allocator.h"

#include <vector>

namespace wasm {

template <typename T>
using arena_vector = std::vector<T, alloc::TemplateAllocator<T>>;

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_ADT_ARENA_VECTOR_H
