// -*- c++ -*-
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

#include "stream/PageCursor.h"

#include "stream/Page.h"
#include "stream/Queue.h"

namespace wasm {

namespace decode {

PageCursor::PageCursor() : CurAddress(0) {
}

PageCursor::PageCursor(Queue* Que)
    : CurPage(Que->FirstPage), CurAddress(Que->FirstPage->getMinAddress()) {
  assert(CurPage);
}

PageCursor::PageCursor(std::shared_ptr<Page> CurPage, size_t CurAddress)
    : CurPage(CurPage), CurAddress(CurAddress) {
  assert(CurPage);
}

PageCursor::PageCursor(const PageCursor& PC)
    : CurPage(PC.CurPage), CurAddress(PC.CurAddress) {
  //    assert(CurPage);
}

PageCursor::~PageCursor() {
}

void PageCursor::assign(const PageCursor& C) {
  CurPage = C.CurPage;
  CurAddress = C.CurAddress;
}

void PageCursor::swap(PageCursor& C) {
  std::swap(CurPage, C.CurPage);
  std::swap(CurAddress, C.CurAddress);
}

size_t PageCursor::getMinAddress() const {
  return CurPage ? CurPage->getMinAddress() : 0;
}

size_t PageCursor::getMaxAddress() const {
  return CurPage ? CurPage->getMaxAddress() : 0;
}

bool PageCursor::isValidPageAddress(size_t Address) {
  return getMinAddress() <= Address && Address < getMaxAddress();
}

size_t PageCursor::getRelativeAddress() const {
  return CurAddress - CurPage->getMinAddress();
}

void PageCursor::setMaxAddress(size_t Address) {
  CurPage->setMaxAddress(Address);
}

bool PageCursor::isIndexAtEndOfPage() const {
  return getCurAddress() == getMaxAddress();
}

uint8_t* PageCursor::getBufferPtr() {
  assert(CurPage);
  return CurPage->getByteAddress(getRelativeAddress());
}

Page* PageCursor::getCurPage() const {
  return CurPage.get();
}

#if 0
namespace {

void describePage(FILE* File, Page* Pg) {
  if (Pg == nullptr)
    fprintf(File, " nullptr");
  else
    Pg->describe(File);
}

} // end of anonymous namespace
#endif

FILE* PageCursor::describe(FILE* File, bool IncludePage) {
  AddressType Addr(CurAddress);
  describeAddress(File, Addr);
  if (IncludePage)
    describePage(File, CurPage.get());
  return File;
}

}  // end of namespace decode

}  // end of namespace wasm
