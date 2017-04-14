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

// Implements a stream reader for wasm/casm files.

#include "interp/ByteReader.h"

#include "interp/ByteReadStream.h"
#include "interp/ReadStream.h"
#include "sexp/Ast.h"
#include "utils/Casting.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace interp {

// Implemented separately so that details are not exposed to users of a
// ByteReader.
class ByteReader::TableHandler {
  TableHandler() = delete;
  TableHandler(const TableHandler&) = delete;
  TableHandler& operator=(const TableHandler&) = delete;

 public:
  explicit TableHandler(ByteReader& Reader) : Reader(Reader) {}
  ~TableHandler() {}

  bool tablePush(IntType Value) {
    TableType::iterator Iter = Table.find(Value);
    if (Iter == Table.end()) {
      Table[Value] = Reader.ReadPos;
      RestoreStack.push_back(false);
    } else {
      if (!Reader.pushPeekPos())
        return false;
      Reader.ReadPos = Iter->second;
      RestoreStack.push_back(true);
    }
    return true;
  }

  bool tablePop() {
    if (RestoreStack.empty())
      return false;
    bool Restore = RestoreStack.back();
    RestoreStack.pop_back();
    if (Restore)
      if (!Reader.popPeekPos())
        return false;
    return true;
  }

 private:
  ByteReader& Reader;
  std::vector<bool> RestoreStack;
  // The map of read cursors associated with table indices.
  typedef std::map<decode::IntType, decode::BitReadCursor> TableType;
  TableType Table;
};

ByteReader::ByteReader(std::shared_ptr<decode::Queue> StrmInput)
    : Reader(true),
      ReadPos(StreamType::Byte, StrmInput),
      Input(std::make_shared<ByteReadStream>()),
      FillPos(0),
      SavedPosStack(SavedPos),
      TblHandler(nullptr) {
}

ByteReader::~ByteReader() {
  delete TblHandler;
}

void ByteReader::setReadPos(const decode::BitReadCursor& StartPos) {
  ReadPos = StartPos;
}

TraceContextPtr ByteReader::getTraceContext() {
  return ReadPos.getTraceContext();
}

namespace {

// Headroom is used to guarantee that several (integer) reads
// can be done in a single iteration of the loop.
constexpr size_t kResumeHeadroom = 100;

}  // end of anonymous namespace

bool ByteReader::canProcessMoreInputNow() {
  FillPos = ReadPos.fillSize();
  if (!ReadPos.isEofFrozen()) {
    if (FillPos < ReadPos.getCurAddress() + kResumeHeadroom)
      return false;
    FillPos -= kResumeHeadroom;
  }
  return true;
}

bool ByteReader::stillMoreInputToProcessNow() {
  return ReadPos.getCurAddress() <= FillPos;
}

BitReadCursor& ByteReader::getPos() {
  return ReadPos;
}

bool ByteReader::atInputEob() {
  return ReadPos.atEob();
}

bool ByteReader::atInputEof() {
  return ReadPos.atEof();
}

bool ByteReader::pushPeekPos() {
  SavedPosStack.push(ReadPos);
  return true;
}

bool ByteReader::popPeekPos() {
  if (SavedPosStack.empty())
    return false;
  ReadPos = SavedPos;
  SavedPosStack.pop();
  return true;
}

decode::StreamType ByteReader::getStreamType() {
  return Input->getType();
}

bool ByteReader::processedInputCorrectly(bool CheckForEof) {
  return (!CheckForEof || ReadPos.atEof()) && ReadPos.isQueueGood();
}

bool ByteReader::readBlockEnter() {
  // Force alignment before processing, in case non-byte encodings
  // are used.
  alignToByte();
  const uint32_t OldSize = Input->readBlockSize(ReadPos);
  TRACE(uint32_t, "block size", OldSize);
  Input->pushEobAddress(ReadPos, OldSize);
  return true;
}

bool ByteReader::readBlockExit() {
  // Force alignment before processing, in case non-byte encodings
  alignToByte();
  ReadPos.popEobAddress();
  return true;
}

bool ByteReader::readBinary(const Node* Eval, IntType& Value) {
  Value = 0;
  if (!isa<BinaryEval>(Eval))
    return false;
  const Node* Encoding = cast<BinaryEval>(Eval)->getKid(0);
  while (1) {
    switch (Encoding->getType()) {
      case NodeType::BinaryAccept:
        Value = cast<BinaryAccept>(Encoding)->getValue();
        return true;
      case NodeType::BinarySelect:
        Encoding = const_cast<Node*>(Encoding->getKid(ReadPos.readBit()));
        break;
      default:
        return false;
    }
  }
  return false;
}

void ByteReader::readFillStart() {
  FillCursor = ReadPos;
}

void ByteReader::readFillMoreInput() {
  if (FillCursor.atEof())
    return;
  FillCursor.advance(PageSize);
}

bool ByteReader::alignToByte() {
  ReadPos.alignToByte();
  return true;
}

uint8_t ByteReader::readBit() {
  return Input->readBit(ReadPos);
}

uint8_t ByteReader::readUint8() {
  return Input->readUint8(ReadPos);
}

uint32_t ByteReader::readUint32() {
  return Input->readUint32(ReadPos);
}

uint64_t ByteReader::readUint64() {
  return Input->readUint64(ReadPos);
}

int32_t ByteReader::readVarint32() {
  return Input->readVarint32(ReadPos);
}

int64_t ByteReader::readVarint64() {
  return Input->readVarint64(ReadPos);
}

uint32_t ByteReader::readVaruint32() {
  return Input->readVaruint32(ReadPos);
}

uint64_t ByteReader::readVaruint64() {
  return Input->readVaruint64(ReadPos);
}

bool ByteReader::tablePush(IntType Value) {
  if (TblHandler == nullptr)
    TblHandler = new TableHandler(*this);
  return TblHandler->tablePush(Value);
}

bool ByteReader::tablePop() {
  if (TblHandler == nullptr)
    return false;
  return TblHandler->tablePop();
}

void ByteReader::describePeekPosStack(FILE* File) {
  if (SavedPosStack.empty())
    return;
  fprintf(File, "*** Saved Pos Stack ***\n");
  fprintf(File, "**********************\n");
  for (const auto& Pos : SavedPosStack.iterRange(1))
    fprintf(File, "@%" PRIxMAX "\n", uintmax_t(Pos.getCurAddress()));
  fprintf(File, "**********************\n");
}

}  // end of namespace interp

}  // end of namespace wasm
