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
#include <utility>

namespace wasm {

namespace alloc {

// Default alignment to use (Starts at 3 => alignment 8).
extern size_t DefaultAllocAlignLog2;

// Defines the base of all allocators. Provides a common API that dispatches to
// the derived class the corresponding (obvious) top-level methods.
//
// Note: byte alignment is in base log2.
template <typename DerivedClass>
class AllocatorBase {
  AllocatorBase (const AllocatorBase<DerivedClass> &) = delete;
  AllocatorBase<DerivedClass>
  &operator=(const AllocatorBase<DerivedClass> &) = delete;
public:
  AllocatorBase() {}

  // Generic allocation
  void *allocateBlock(size_t Size, size_t AlignLog2=DefaultAllocAlignLog2) {
    return
        static_cast<DerivedClass *>(this)->allocateBlock(Size, AlignLog2);
  }

  // Generic deallocation
  void deallocateBlock(void *Pointer) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Pointer);
  }

  // Allocate a single element of Type
  template<class Type>
  Type *allocate(size_t AlignLog2=DefaultAllocAlignLog2) {
    return
        static_cast<Type *>(
            static_cast<DerivedClass *>(this)
            ->allocateBlock(sizeof(Type), AlignLog2));
  }

  // Allocate and construct an instance of Type T using T(Args).
  template<typename T, typename... Args>
  T *create(Args&&... args)
  {
    return new (allocate<T>()) T(std::forward<Args>(args)...);
  }

  // Allocate an array of Type.
  template<class Type, size_t Size>
  Type *allocate(size_t AlignLog2=DefaultAllocAlignLog2) {
    return static_cast<DerivedClass *>(this)
        ->allocateBlock(sizeof(Type) * Size, AlignLog2);
  }

  // Allocate and construct an instance of Type[Size];
  template<typename T, size_t Size>
  T *create() {
    return new (allocate<T, Size>()) T[Size];
  }

  // Allocate an array of Type[Size] (alternate form)
  template<class Type>
  Type *allocateArray(size_t Size, size_t AlignLog2=DefaultAllocAlignLog2) {
    return static_cast<DerivedClass *>(this)
        ->allocateBlock(sizeof(Type) * Size, AlignLog2);
  }

  // Allocate and construct an instance of Type[Size] (alternate form)
  template<class Type>
  Type *createArray(const size_t Size, size_t AlignLog2=DefaultAllocAlignLog2) {
    return new (allocateArray<Type>(Size, AlignLog2)) Type[Size];
  }


  // Deallcate an instance of Type
  template<class Type>
  void deallocate(Type * Pointer) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Pointer);
  }

  // Destruct and deallocate an instance of Type
  template<class Type>
  void destroy(Type *Pointer) {
    Pointer->~Type();
    deallocate<Type>(Pointer);
  }

  // Deallocate an array of Type
  template<class Type, size_t Size>
  void deallocate(Type *Pointer) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Pointer);
  }

  // Destruct and deallocate an array of Type[Size]
  template<class Type, size_t Size>
  void destroy(Type *Pointer) {
    for (size_t i = 0; i < Size; ++i)
      (Pointer + i)->~Type();
    deallocate<Type, Size>(Pointer);
  }

  // Deallocate an array of Type (alternate form)
  template<class Type>
  void deallocateArray(Type *Pointer, size_t) {
    static_cast<DerivedClass *>(this)->deallocateBlock(Pointer);
  }

  // Eestruct and deallocate an array of Type[Size] (alternate form)
  template<class Type>
  void destroyArray(Type *Pointer, size_t Size) {
    for (size_t i = 0; i < Size; ++i)
      (Pointer + i)->~Type();
    deallocateArray<Type>(Pointer, Size);
  }
};

// Defines an allocator based on malloc
class Malloc : public AllocatorBase<Malloc> {
  Malloc(const Malloc &) = delete;
  Malloc &operator=(const Malloc&) = delete;
public:
  Malloc() {}
  void *allocateBlock(size_t Size, size_t AlignLog2=DefaultAllocAlignLog2) {
    (void) AlignLog2;
    return malloc(Size);
  }

  void deallocateBlock(void* Pointer) {
    free(Pointer);
  }
};

extern size_t DefaultArenaInitPageSize;
extern size_t DefaultArenaMaxPageSize;
extern size_t DefaultArenaGrowAfterCount;
extern size_t DefaultArenaThreshold;

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
  void deallocateBlock(void */*Pointer*/) = delete;
public:
  explicit ArenaAllocator(BaseAllocator &_BaseAlloc,
                          size_t _InitPageSize = DefaultArenaInitPageSize,
                          size_t _MaxPageSize = DefaultArenaMaxPageSize,
                          size_t _Threshold = DefaultArenaThreshold,
                          size_t _GrowAfterCount = DefaultArenaGrowAfterCount) :
      BaseAlloc(_BaseAlloc),
      Threshold(_Threshold),
      PageSize(_InitPageSize),
      MaxPageSize(_MaxPageSize),
      GrowAfterCount(_GrowAfterCount)
      {}


  void *allocateBlock(size_t Size, size_t AlignLog2=DefaultAllocAlignLog2) {
    assert(AlignLog2 < 32);
    size_t Alignment = (size_t)1 << AlignLog2;
    size_t WantedSize = Size + Alignment - 1; // Pad to guarantee enough space.

    // Make sure alignment doesn't cause size overflow.
    assert(WantedSize >= Size);

    if ((WantedSize >= Threshold) || (WantedSize >= PageSize)) {
      void *Page = BaseAlloc.allocateBlock(Size, Alignment);
      BigAllocations.emplace_back(Page);
      return Page;
    }

    size_t AlignBytes = alignmentBytesNeeded(Alignment);
    WantedSize = AlignBytes + Size;

    if (WantedSize <= size_t(End - Available)) {
      void *Space = Available + AlignBytes;
      Available += WantedSize;
      return Space;
    }

    createNewPage();
    AlignBytes = alignmentBytesNeeded(Alignment);
    WantedSize = AlignBytes + Size;
    void *Space = Available + AlignBytes;
    Available += WantedSize;
    return Space;
  }

  ~ArenaAllocator() {
    for (void *Page : AllocatedPages)
      BaseAlloc.deallocateBlock(Page);
    for (void *Page : BigAllocations)
      BaseAlloc.deallocateBlock(Page);
  }

private:
  BaseAllocator &BaseAlloc;
  size_t Threshold;
  size_t PageSize;
  size_t MaxPageSize;
  size_t GrowAfterCount;
  std::vector<void*> AllocatedPages;
  // Big allocations not put into allocated pages.
  std::vector<void*> BigAllocations;
  // The next two fields hold available space.
  uint8_t *Available;
  uint8_t *End;

  size_t alignmentBytesNeeded(size_t Alignment) {
    return (((size_t)(Available) + Alignment - 1)
            & ~(Alignment - 1)) - (size_t)Available;
  }

  void createNewPage() {
    size_t NumBumpPages = AllocatedPages.size();
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
    AllocatedPages.emplace_back(Page);
    Available = Page;
    End = Available + PageSize;
  }
};

// Implements an arena allocator using a Malloc allocator.
class MallocArena : public AllocatorBase<MallocArena> {
  MallocArena(const MallocArena &) = delete;
  MallocArena &operator=(const MallocArena &) = delete;
public:
  explicit MallocArena(size_t InitPageSize = DefaultArenaInitPageSize,
                       size_t MaxPageSize = DefaultArenaMaxPageSize,
                       size_t Threshold = DefaultArenaThreshold,
                       size_t GrowAfterCount = DefaultArenaGrowAfterCount);

  void *allocateBlock(size_t Size, size_t AligLog2=DefaultAllocAlignLog2);

  void deallocateBlock(void * *Pointer) = delete;

private:
  Malloc Allocator;
  ArenaAllocator<Malloc> Arena;
};

} // end of namespace alloc

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_ALLOCATOR_H
