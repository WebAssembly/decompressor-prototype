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

// implements a reader to extract filter sections.

#include "binary/BinaryReader.h"

#include <cstdio>

using namespace wasm::decode;
using namespace wasm::utils;

namespace wasm {

namespace filt {

bool BinaryReader::isBinary(const char* Filename) {
  FILE* File = fopen(Filename, "r");
  if (File == nullptr)
    return false;
  // Read first 4 bytes and generate magic number.
  uint32_t Number = 0;
  uint32_t Shift = 0;
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    uint8_t Byte = fgetc(File);
    Number |= (Byte << Shift);
    Shift += CHAR_BIT;
  }
  fclose(File);
  return Number == WasmBinaryMagic;
}

BinaryReader::BinaryReader(decode::ByteQueue* Input, SymbolTable& Symtab)
    : Alloc(Symtab.getAllocator()),
      ReadPos(StreamType::Byte, Input),
      SectionSymtab(Symtab),
      Trace(ReadPos, "BinaryReader") {
  Reader = Alloc->create<ByteReadStream>();
}

template <class T>
void BinaryReader::readNullary() {
  auto* Node = Alloc->create<T>();
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnary() {
  if (NodeStack.size() < 1)
    fatal("Can't find arguments for s-expression");
  Node* Arg = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Alloc->create<T>(Arg);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnarySymbol() {
  auto* Symbol = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto* Node = Alloc->create<T>(Symbol);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnaryUint8() {
  auto* Number = Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
  auto* Node = Alloc->create<T>(Number);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnaryVarint32() {
  auto* Number = Alloc->create<IntegerNode>(Reader->readVarint32(ReadPos));
  auto* Node = Alloc->create<T>(Number);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnaryVarint64() {
  auto* Number = Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
  auto* Node = Alloc->create<T>(Number);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnaryVaruint32() {
  auto* Number = Alloc->create<IntegerNode>(Reader->readVaruint32(ReadPos));
  auto* Node = Alloc->create<T>(Number);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnaryVaruint64() {
  auto* Number = Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
  auto* Node = Alloc->create<T>(Number);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readBinary() {
  if (NodeStack.size() < 2)
    fatal("Can't find arguments for s-expression");
  Node* Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Alloc->create<T>(Arg1, Arg2);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readBinarySymbol() {
  auto* Symbol = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto* Body = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Alloc->create<T>(Symbol, Body);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readTernary() {
  if (NodeStack.size() < 3)
    fatal("Can't find arguments for s-expression");
  Node* Arg3 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node* Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Alloc->create<T>(Arg1, Arg2, Arg3);
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readNary() {
  uint32_t NumKids = Reader->readVaruint32(ReadPos);
  size_t StackSize = NodeStack.size();
  if (StackSize < NumKids)
    fatal("Can't find arguments for s-expression");
  auto* Node = Alloc->create<T>();
  for (size_t i = StackSize - NumKids; i < StackSize; ++i)
    Node->append(NodeStack[i]);
  for (uint32_t i = 0; i < NumKids; ++i)
    NodeStack.pop_back();
  Trace.traceSexp(Node);
  NodeStack.push_back(Node);
}

FileNode* BinaryReader::readFile() {
  TraceClass::Method _("readFile", Trace);
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  Trace.traceUint32_t("MagicNumber", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to read, did not find WASM binary magic number");
  Version = Reader->readUint32(ReadPos);
  Trace.traceHexUint32_t("Version", Version);
  auto* File = Alloc->create<FileNode>();
  while (!ReadPos.atEob())
    File->append(readSection());
  Trace.traceSexp(File);
  return File;
}

SectionNode* BinaryReader::readSection() {
  TraceClass::Method _("readSection", Trace);
  ExternalName Name = readExternalName();
  auto* SectionName = Alloc->create<SymbolNode>(Name);
  auto* Section = Alloc->create<SectionNode>();
  Section->append(SectionName);
  Trace.traceString("Name", Name);
  size_t StartStackSize = NodeStack.size();
  readBlock([&]() {
    if (Name == "filter") {
      readSymbolTable();
      while (!ReadPos.atEob())
        readNode();
    } else {
      // TODO(karlschimpf) Fix to actually read!
      Trace.traceMessage("Skipping unknown Section" + Name);
      fatal("Handling non-filter sections not implemented yet!");
    }
  });
  size_t StackSize = NodeStack.size();
  if (StackSize < StartStackSize)
    fatal("Malformed section: " + Name);
  for (size_t i = StartStackSize; i < StackSize; ++i)
    Section->append(NodeStack[i]);
  for (size_t i = StartStackSize; i < StackSize; ++i)
    NodeStack.pop_back();
  Trace.traceSexp(Section);
  return Section;
}

void BinaryReader::readSymbolTable() {
  TraceClass::Method _("readSymbolTable", Trace);
  SectionSymtab.clear();
  uint32_t NumSymbols = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NumSymbols; ++i) {
    ExternalName& Name = readExternalName();
    Trace.traceUint32_t("Index", i);
    Trace.traceString("Symbol", Name);
    SectionSymtab.addSymbol(Name);
  }
}

ExternalName& BinaryReader::readExternalName() {
  static ExternalName Name;
  Name.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Name.push_back(char(Byte));
  }
  return Name;
}

InternalName& BinaryReader::readInternalName() {
  static InternalName Name;
  Name.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Name.push_back(char(Byte));
  }
  return Name;
}

void BinaryReader::readBlock(std::function<void()> ApplyFn) {
  TraceClass::Method _("readBlock", Trace);
  const size_t BlockSize = Reader->readVaruint32(ReadPos);
  Trace.traceSize_t("Block size", BlockSize);
  ReadPos.pushEobAddress(ReadPos.getCurByteAddress() + BlockSize);
  ApplyFn();
  ReadPos.popEobAddress();
}

void BinaryReader::readNode() {
  TraceClass::Method _("readNode", Trace);
  switch (NodeType Opcode = (NodeType)Reader->readUint8(ReadPos)) {
    case OpAnd:
      readBinary<AndNode>();
      break;
    case OpBlock:
      readUnary<BlockNode>();
      break;
    case OpBlockEndNoArgs:
      readNullary<BlockEndNoArgsNode>();
      break;
    case OpByteToByte:
      readUnary<ByteToByteNode>();
      break;
    case OpCase: {
      auto* Key = Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
      Node* Code = NodeStack.back();
      NodeStack.pop_back();
      auto* Case = Alloc->create<CaseNode>(Key, Code);
      Trace.traceSexp(Case);
      NodeStack.push_back(Case);
      break;
    }
    case OpDefault:
      readBinarySymbol<DefaultNode>();
      break;
    case OpDefine:
      readBinarySymbol<DefineNode>();
      break;
    case OpError:
      readNullary<ErrorNode>();
      break;
    case OpEval:
      readUnarySymbol<EvalNode>();
      break;
    case OpEvalDefault:
      readUnarySymbol<EvalDefaultNode>();
      break;
    case OpFilter:
      readNary<FilterNode>();
      break;
    case OpIfThen:
      readBinary<IfThenNode>();
      break;
    case OpIfThenElse:
      readTernary<IfThenElseNode>();
      break;
    case OpIsByteIn:
      readNullary<IsByteInNode>();
      break;
    case OpIsByteOut:
      readNullary<IsByteOutNode>();
      break;
    case OpI32Const:
      readUnaryVarint32<I32ConstNode>();
      break;
    case OpI64Const:
      readUnaryVarint64<I64ConstNode>();
      break;
    case OpLoop:
      readBinary<LoopNode>();
      break;
    case OpLoopUnbounded:
      readUnary<LoopUnboundedNode>();
      break;
    case OpOr:
      readBinary<OrNode>();
      break;
    case OpPeek:
      readUnary<PeekNode>();
      break;
    case OpNot:
      readUnary<NotNode>();
      break;
    case OpMap:
      readBinary<MapNode>();
      break;
    case OpRead:
      readUnary<ReadNode>();
      break;
    case OpOpcode:
      readNary<OpcodeNode>();
      cast<OpcodeNode>(NodeStack.back())->installFastLookup();
      break;
    case OpSelect:
      readNary<SelectNode>();
      cast<SelectNode>(NodeStack.back())->installFastLookup();
      break;
    case OpSequence:
      readNary<SequenceNode>();
      break;
    case OpUint32NoArgs:
      readNullary<Uint32NoArgsNode>();
      break;
    case OpUint32OneArg:
      readUnaryUint8<Uint32OneArgNode>();
      break;
    case OpUint64NoArgs:
      readNullary<Uint64NoArgsNode>();
      break;
    case OpUint64OneArg:
      readUnaryUint8<Uint64OneArgNode>();
      break;
    case OpUint8NoArgs:
      readNullary<Uint8NoArgsNode>();
      break;
    case OpUint8OneArg:
      readUnaryUint8<Uint8OneArgNode>();
      break;
    case OpUndefine:
      readUnary<UndefineNode>();
      break;
    case OpU32Const:
      readUnaryVaruint32<U32ConstNode>();
      break;
    case OpU64Const:
      readUnaryVaruint64<U64ConstNode>();
      break;
    case OpVarint32NoArgs:
      readNullary<Varint32NoArgsNode>();
      break;
    case OpVarint32OneArg:
      readUnaryUint8<Varint32OneArgNode>();
      break;
    case OpVarint64NoArgs:
      readNullary<Varint64NoArgsNode>();
      break;
    case OpVarint64OneArg:
      readUnaryUint8<Varint64OneArgNode>();
      break;
    case OpVaruint32NoArgs:
      readNullary<Varuint32NoArgsNode>();
      break;
    case OpVaruint32OneArg:
      readUnaryUint8<Varuint32OneArgNode>();
      break;
    case OpVaruint64NoArgs:
      readNullary<Varuint64NoArgsNode>();
      break;
    case OpVaruint64OneArg:
      readUnary<Varuint64OneArgNode>();
      break;
    case OpVersion:
      readUnaryVaruint32<VersionNode>();
      break;
    case OpVoid:
      readNullary<VoidNode>();
      break;
    case OpLastRead:
      readNullary<LastReadNode>();
      break;
    case NO_SUCH_NODETYPE:
    case OpInteger:
    case OpFile:
    case OpSection:
    case OpSymbol:
    case OpUnknownSection:
      Trace.traceHexUint32_t("Opcode", Opcode);
      fatal("Uses construct not implemented yet!");
      break;
  }
}

}  // end of namespace filt

}  // end of namespace wasm
