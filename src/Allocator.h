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

// Defines generic allocators for building arena allocators etc.

#ifndef DECOMPRESSOR_SRC_ALLOCATOR_H
#define DECOMPRESSOR_SRC_ALLOCATOR_H

#include "Defs.h"

#include <cstdlib>
#include <vector>

namespace wasm {

namespace alloc {

// Defines the base of all allocators. Provides a common API that dispatches to
// the derived class the corresponding (obvious) top-level methods.
//
// Note: byte alignment is in base log2.
template <typename DerivedClass>
class AllocatorBase {
  AllocatorBase (const AllocatorBase<DerivedClass> &) = delete;
  AllocatorBase<DerivedClass> &operator=(const AllocatorBase<DerivedClass> &) = delete;
public:
  AllocatorBase() {}
  // Generic allocation
  void *allocateBlock(size_t Size, size_t Alignment=1) {
    return
        static_cast<DerivedClass *>(this)->allocateBlock(Size, Alignment);
  }

  // Allocate a single element of Type
  template<class Type>
  Type *allocate(size_t Alignment=1) {
    return
        static_cast<Type *>(
            static_cast<DerivedClass *>(this)
            ->allocateBlock(sizeof(Type), Alignment));
  }

  // Allocate an array of Type.
  template<class Type, size_t Size>
  Type *allocate(size_t Alignment=1) {
    return static_cast<DerivedClass *>(this)
        ->allocateBlock(sizeof(Type) * Size, Alignment);
  }

  // Allocate an array of Type (alternate form)
  template<class Type>
  Type *allocateArray(size_t Size, size_t Alignment) {
    return static_cast<DerivedClass *>(this)
        ->allocateBlock(sizeof(Type) * Size, Alignment);
  }

  // Generic deallocation
  void deallocateBlock(size_t Size, size_t Alignment=1) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Size, Alignment);
  }

  // Deallcate a single element of Type
  template<class Type>
  void deallocate(const Type * Pointer) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Pointer, sizeof(Type));
  }

  // Deallocate an array of Type
  template<class Type, size_t Size>
  void deallocate(const Type *Pointer) {
    static_cast<DerivedClass *>(this)
        ->deallocateBlock(Pointer, sizeof(Type) * Size);
  }

  // deallocate an arra of Type (alternate form)
  template<class Type>
  void deallocateArray(const Type *Pointer, size_t Size) {
    static_cast<DerivedClass *>(this)
        ->deallocateBlock(Pointer, sizeof(Type) * Size);
  }
};

// Defines an allocator based on malloc
class MallocAllocator : public AllocatorBase<MallocAllocator> {
  MallocAllocator(const MallocAllocator &) = delete;
  MallocAllocator &operator=(const MallocAllocator&) = delete;
public:
  MallocAllocator() {}
  void *allocateBlock(size_t Size, size_t Alignment=1) {
    (void) Alignment;
    return malloc(Size);
  }

  void deallocateBlock(void* Pointer, size_t /* Size */) {
    free(Pointer);
  }

  // Pull in base class overloads.
  using AllocatorBase<MallocAllocator>::allocate;
  using AllocatorBase<MallocAllocator>::deallocate;
};

// Defines an simple arena allocator, that will allocate from internal pages if
// smaller than threshold. Otherwise will allocate individually through base
// allocator.
//
// WARNING: Current arena allocator does not call destructors of allocated
// objects.
template<class BaseAllocator>
class ArenaAllocator : public AllocatorBase<ArenaAllocator<BaseAllocator>> {
  ArenaAllocator(const ArenaAllocator &) = delete;
  ArenaAllocator &operator=(const ArenaAllocator&) = delete;
  ArenaAllocator() = delete;
  static constexpr size_t DefaultInitPageSize = size_t(1) << 10;
  static constexpr size_t DefaultMaxPageSize = size_t(1) << 20;
  static constexpr size_t DefaultGrowAfterCount = 32;
public:
  explicit ArenaAllocator(BaseAllocator &_BaseAlloc,
                          size_t _Threshold = DefaultInitPageSize,
                          size_t _InitPageSize = DefaultInitPageSize,
                          size_t _MaxPageSize = DefaultMaxPageSize,
                          size_t _GrowAfterCount = DefaultGrowAfterCount) :
      BaseAlloc(_BaseAlloc),
      Threshold(_Threshold),
      PageSize(_InitPageSize),
      MaxPageSize(_MaxPageSize),
      GrowAfterCount(_GrowAfterCount)
      {}

  void *allocateBlock(size_t Size, size_t Alignment=1) {
    assert(Alignment > 0);
    assert(Alignment < 32);
    Alignment = (size_t)1 << Alignment;
    size_t AlignBytes = alignmentBytesNeeded(Alignment);
    size_t WantedSize = AlignBytes + Size;

    // Make sure alignment doesn't cause size overflow.
    assert(WantedSize >= Size);

    if (WantedSize <= size_t(End - Available)) {
      void *Space = Available + AlignBytes;
      Available += WantedSize;
      return Space;
    }

    WantedSize = Size + Alignment - 1; // Pad to guarantee enough space.
    if ((WantedSize >= Threshold) || (WantedSize >= PageSize)) {
      void *Space = BaseAlloc.allocateBlock(Size, Alignment);
      BigAllocations.emplace_back(Space, Size);
      return Space;
    }

    createNewBumpPage();
    AlignBytes = alignmentBytesNeeded(Alignment);
    WantedSize = AlignBytes + Size;
    void *Space = Available + AlignBytes;
    Available += WantedSize;
    return Space;
  }

  void deallocateBlock(void * /*Pointer*/, size_t /*Size*/) {}

  ~ArenaAllocator() {
    for (const auto &pair : BumpPages)
      BaseAlloc.deallocateBlock(pair.first, pair.second);
    for (const auto &pair : BigAllocations)
      BaseAlloc.deallocateBlock(pair.first, pair.second);
  }

  // Pull in base class overloads.
  using AllocatorBase<ArenaAllocator<BaseAllocator>>::allocate;
  using AllocatorBase<ArenaAllocator<BaseAllocator>>::deallocate;
private:
  BaseAllocator &BaseAlloc;
  size_t Threshold;
  size_t PageSize;
  size_t MaxPageSize;
  size_t GrowAfterCount;
  std::vector<std::pair<void*, size_t>> BumpPages;
  // Big allocations not put into Slabs.
  std::vector<std::pair<void*, size_t>> BigAllocations;
  // The next two fields hold available space.
  uint8_t *Available;
  uint8_t *End;

  size_t alignmentBytesNeeded(size_t Alignment) {
    return (((size_t)(Available) + Alignment - 1)
            & ~(Alignment - 1)) - (size_t)Available;
  }

  void createNewBumpPage() {
    size_t NumBumpPages = BumpPages.size();
    // Scale page size as more bump pages are needed, to cut down
    // on number of page allocations.
    size_t GrowSize = size_t(1) << (NumBumpPages / GrowAfterCount);
    if (GrowSize > PageSize) {
      if (GrowSize > MaxPageSize)
        GrowSize = MaxPageSize;
      PageSize = GrowSize;
    }
    size_t BlockSize = sizeof(uint8_t) * PageSize;
    uint8_t *Page = static_cast<uint8_t *>(BaseAlloc.allocateBlock(BlockSize));
    BumpPages.emplace_back(Page, BlockSize);
    Available = Page;
    End = Available + PageSize;
  }
};

} // end of namespace alloc

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_ALLOCATOR_H
