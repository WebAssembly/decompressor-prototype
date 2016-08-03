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

// Implements generic allocators

#include "Allocator.h"

namespace wasm {

namespace alloc {

size_t DefaultAllocAlignLog2 = 3;

size_t DefaultArenaInitPageSize = size_t(1) << 12;
size_t DefaultArenaMaxPageSize = size_t(1) << 20;
size_t DefaultArenaGrowAfterCount = 4;
size_t DefaultArenaThreshold = size_t(1) << 10;

Malloc::~Malloc() {
}

namespace {
Malloc MallocAllocator;
}  // end of anonymous namespace

Allocator* Allocator::Default = &MallocAllocator;

void* Malloc::allocateVirtual(size_t Size, size_t AlignLog2) {
  return allocateDispatch(Size, AlignLog2);
}

void Malloc::deallocateVirtual(void* Pointer) {
  deallocateDispatch(Pointer);
}

// template<> class ArenaAllocator<Malloc>;

MallocArena::MallocArena(size_t InitPageSize,
                         size_t MaxPageSize,
                         size_t Threshold,
                         size_t GrowAfterCount)
    : AllocatorBase<MallocArena>(),
      Allocator(),
      Arena(Allocator, InitPageSize, MaxPageSize, Threshold, GrowAfterCount) {
}

void* MallocArena::allocateVirtual(size_t Size, size_t AlignLog2) {
  return allocateDispatch(Size, AlignLog2);
}

void MallocArena::deallocateVirtual(void*) {
  // Can't deallocate, do nothing.
}

void* MallocArena::allocateDispatch(size_t Size, size_t AlignLog2) {
  return Arena.allocateDispatch(Size, AlignLog2);
}

}  // end of namespace alloc

}  // end of namespace wasm
