// -*- C++ -*-
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

#include "stream/Cursor.h"

#include "stream/BlockEob.h"
#include "stream/Page.h"
#include "stream/Queue.h"

namespace wasm {

using namespace utils;

namespace decode {

Cursor::TraceContext::TraceContext(Cursor& Pos) : Pos(Pos) {
}
Cursor::TraceContext::~TraceContext() {
}

Cursor::Cursor(StreamType Type, std::shared_ptr<Queue> Que)
    : PageCursor(Que->FirstPage, Que->FirstPage->getMinAddress()),
      Type(Type),
      Que(Que),
      EobPtr(Que->getEofPtr()) {
  updateGuaranteedBeforeEob();
}

Cursor::Cursor(const Cursor& C)
    : PageCursor(C),
      Type(C.Type),
      Que(C.Que),
      EobPtr(C.EobPtr),
      CurByte(C.CurByte) {
  updateGuaranteedBeforeEob();
}

Cursor::Cursor(const Cursor& C, AddressType StartAddress, bool ForRead)
    : PageCursor(C),
      Type(C.Type),
      Que(C.Que),
      EobPtr(C.EobPtr),
      CurByte(C.CurByte) {
  CurPage = ForRead ? Que->getReadPage(StartAddress)
                    : Que->getWritePage(StartAddress);
  CurAddress = StartAddress;
  updateGuaranteedBeforeEob();
}

Cursor::Cursor() : PageCursor(), Type(StreamType::Byte) {
}

Cursor::~Cursor() {
}

bool Cursor::atEof() const {
  return CurAddress == Que->getEofAddress();
}

void Cursor::swap(Cursor& C) {
  PageCursor::swap(C);
  std::swap(Type, C.Type);
  std::swap(Que, C.Que);
  std::swap(EobPtr, C.EobPtr);
  std::swap(CurByte, C.CurByte);
  std::swap(CurByte, C.CurByte);
  std::swap(GuaranteedBeforeEob, C.GuaranteedBeforeEob);
}

void Cursor::assign(const Cursor& C) {
  PageCursor::assign(C);
  Type = C.Type;
  Que = C.Que;
  EobPtr = C.EobPtr;
  CurByte = C.CurByte;
  GuaranteedBeforeEob = C.GuaranteedBeforeEob;
}

bool Cursor::isQueueGood() const {
  return Que->isGood();
}

bool Cursor::isBroken() const {
  return Que->isBroken(*this);
}

std::shared_ptr<Queue> Cursor::getQueue() {
  return Que;
}

bool Cursor::isEofFrozen() const {
  return Que->isEofFrozen();
}

AddressType Cursor::getEofAddress() const {
  return Que->getEofAddress();
}

AddressType& Cursor::getEobAddress() const {
  return EobPtr->getEobAddress();
}

void Cursor::freezeEof() {
  Que->freezeEof(CurAddress);
}

AddressType Cursor::fillSize() {
  return Que->fillSize();
}

void Cursor::TraceContext::describe(FILE* File) {
  Pos.describe(File);
}

TraceContextPtr Cursor::getTraceContext() {
  return std::make_shared<Cursor::TraceContext>(*this);
}

void Cursor::close() {
  CurPage = Que->getErrorPage();
  CurByte = 0;
  GuaranteedBeforeEob = false;
}

void Cursor::updateGuaranteedBeforeEob() {
  GuaranteedBeforeEob =
      CurPage ? std::min(CurPage->getMaxAddress(), EobPtr->getEobAddress()) : 0;
}

void Cursor::fail() {
  Que->fail();
  CurPage = Que->getErrorPage();
  CurAddress = kErrorPageAddress;
  updateGuaranteedBeforeEob();
  EobPtr->fail();
}

bool Cursor::readFillBuffer() {
  if (CurAddress >= Que->getEofAddress())
    return false;
  AddressType BufferSize = Que->readFromPage(CurAddress, PageSize, *this);
  return BufferSize > 0;
}

void Cursor::writeFillBuffer(AddressType WantedSize) {
  if (CurAddress >= Que->getEofAddress()) {
    fail();
    return;
  }
  AddressType BufferSize = Que->writeToPage(CurAddress, WantedSize, *this);
  if (BufferSize == 0)
    fail();
}

FILE* Cursor::describe(FILE* File, bool IncludeDetail, bool AddEoln) {
  if (IncludeDetail)
    fputs("Cursor<", File);
  describeDerivedExtensions(File, IncludeDetail);
  if (IncludeDetail) {
    if (EobPtr->isDefined()) {
      fprintf(File, ", eob=");
      describeAddress(File, getEofAddress());
    }
    fputc('>', File);
  }
  if (AddEoln)
    fputc('\n', File);
  return File;
}

void Cursor::describeDerivedExtensions(FILE* File, bool IncludeDetail) {
  PageCursor::describe(File, IncludeDetail);
}

}  // end of namespace decode

}  // end of namespace wasm
