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
#include "interp/ByteReadStream.h"

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

BinaryReader::BinaryReader(std::shared_ptr<decode::Queue> Input,
                           std::shared_ptr<SymbolTable> Symtab)
    : Reader(std::make_shared<ByteReadStream>()),
      ReadPos(StreamType::Byte, Input),
      Symtab(Symtab),
      SectionSymtab(Symtab),
      Trace(ReadPos, "BinaryReader") {
}

template <class T>
void BinaryReader::readNullary() {
  auto* Node = Symtab->create<T>();
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUnary() {
  if (NodeStack.size() < 1)
    fatal("Can't find arguments for s-expression");
  Node* Arg = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Arg);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readUint8() {
  auto* Node = Symtab->create<T>(Reader->readUint8(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVarint32() {
  auto* Node = Symtab->create<T>(Reader->readVarint32(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVarint64() {
  auto* Node = Symtab->create<T>(Reader->readVarint64(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVaruint32() {
  auto* Node = Symtab->create<T>(Reader->readVaruint32(ReadPos));
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readVaruint64() {
  auto* Node = Symtab->create<T>(Reader->readVarint64(ReadPos));
  TRACE_SEXP(nullptr, Node);
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
  auto* Node = Symtab->create<T>(Arg1, Arg2);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readBinarySymbol() {
  auto* Symbol = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto* Body = NodeStack.back();
  NodeStack.pop_back();
  auto* Node = Symtab->create<T>(Symbol, Body);
  TRACE_SEXP(nullptr, Node);
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
  auto* Node = Symtab->create<T>(Arg1, Arg2, Arg3);
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

template <class T>
void BinaryReader::readNary() {
  uint32_t NumKids = Reader->readVaruint32(ReadPos);
  size_t StackSize = NodeStack.size();
  if (StackSize < NumKids)
    fatal("Can't find arguments for s-expression");
  auto* Node = Symtab->create<T>();
  for (size_t i = StackSize - NumKids; i < StackSize; ++i)
    Node->append(NodeStack[i]);
  for (uint32_t i = 0; i < NumKids; ++i)
    NodeStack.pop_back();
  TRACE_SEXP(nullptr, Node);
  NodeStack.push_back(Node);
}

FileNode* BinaryReader::readFile() {
  TRACE_METHOD("readFile", Trace);
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  TRACE(uint32_t, "MagicNumber", MagicNumber);
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to read, did not find WASM binary magic number");
  Version = Reader->readUint32(ReadPos);
  TRACE(uint32_t, "Version", Version);
  auto* File = Symtab->create<FileNode>();
  while (!ReadPos.atByteEob())
    File->append(readSection());
  TRACE_SEXP(nullptr, File);
  SectionSymtab.install(File);
  return File;
}

SectionNode* BinaryReader::readSection() {
  TRACE_METHOD("readSection", Trace);
  ExternalName Name = readExternalName();
  auto* SectionName = Symtab->create<SymbolNode>(Name);
  auto* Section = Symtab->create<SectionNode>();
  Section->append(SectionName);
  TRACE(string, "Name", Name);
  size_t StartStackSize = NodeStack.size();
  readBlock([&]() {
    if (Name == "filter") {
      readSymbolTable();
      while (!ReadPos.atByteEob())
        readNode();
    } else {
      // TODO(karlschimpf) Fix to actually read!
      TRACE_MESSAGE("Skipping unknown Section" + Name);
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
  TRACE_SEXP(nullptr, Section);
  return Section;
}

void BinaryReader::readSymbolTable() {
  TRACE_METHOD("readSymbolTable", Trace);
  SectionSymtab.clear();
  uint32_t NumSymbols = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NumSymbols; ++i) {
    ExternalName& Name = readExternalName();
    TRACE(uint32_t, "Index", i);
    TRACE(string, "Symbol", Name);
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
  TRACE_METHOD("readBlock", Trace);
  const size_t BlockSize = Reader->readBlockSize(ReadPos);
  TRACE(size_t, "Block size", BlockSize);
  Reader->pushEobAddress(ReadPos, BlockSize);
  ApplyFn();
  ReadPos.popEobAddress();
}

void BinaryReader::readNode() {
  TRACE_METHOD("readNode", Trace);
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
    case OpCase: {
      readBinary<CaseNode>();
      break;
    }
    case OpConvert:
      readTernary<ConvertNode>();
      break;
    case OpDefine: {
      auto* Symbol = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      auto* Body = NodeStack.back();
      NodeStack.pop_back();
      auto* Params = NodeStack.back();
      NodeStack.pop_back();
      auto* Node = Symtab->create<DefineNode>(Symbol, Params, Body);
      TRACE_SEXP(nullptr, Node);
      NodeStack.push_back(Node);
      break;
    }
    case OpRename:
      readBinary<RenameNode>();
      break;
    case OpError:
      readNullary<ErrorNode>();
      break;
    case OpEval: {
      auto* Node = Symtab->create<EvalNode>();
      auto *Sym = SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      Node->append(Sym);
      uint32_t NumParams = Reader->readVaruint32(ReadPos);
      size_t StackSize = NodeStack.size();
      if (StackSize < NumParams)
        fatal("Can't find arguments for s-expression");
      for (size_t i = StackSize - NumParams; i < StackSize; ++i)
        Node->append(NodeStack[i]);
      for (uint32_t i = 0; i < NumParams; ++i)
        NodeStack.pop_back();
      NodeStack.push_back(Node);
      break;
    }
    case OpFilter:
      readNary<FilterNode>();
      break;
    case OpIfThen:
      readBinary<IfThenNode>();
      break;
    case OpIfThenElse:
      readTernary<IfThenElseNode>();
      break;
    case OpI32Const:
      readVarint32<I32ConstNode>();
      break;
    case OpI64Const:
      readVarint64<I64ConstNode>();
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
    case OpParam:
      readVaruint32<ParamNode>();
      break;
    case OpPeek:
      readUnary<PeekNode>();
      break;
    case OpNot:
      readUnary<NotNode>();
      break;
    case OpRead:
      readUnary<ReadNode>();
      break;
    case OpWrite:
      readBinary<WriteNode>();
      break;
    case OpOpcode:
      readNary<OpcodeNode>();
      break;
    case OpMap:
      readNary<MapNode>();
      break;
    case OpSwitch:
      readNary<SwitchNode>();
      break;
    case OpSequence:
      readNary<SequenceNode>();
      break;
    case OpStream: {
      uint8_t Encoding = Reader->readUint8(ReadPos);
      StreamKind StrmKind;
      StreamType StrmType;
      StreamNode::decode(Encoding, StrmKind, StrmType);
      auto* Node = Symtab->create<StreamNode>(StrmKind, StrmType);
      TRACE_SEXP(nullptr, Node);
      NodeStack.push_back(Node);
      break;
    }
    case OpUint32NoArgs:
      readNullary<Uint32NoArgsNode>();
      break;
    case OpUint32OneArg:
      readUint8<Uint32OneArgNode>();
      break;
    case OpUint64NoArgs:
      readNullary<Uint64NoArgsNode>();
      break;
    case OpUint64OneArg:
      readUint8<Uint64OneArgNode>();
      break;
    case OpUint8NoArgs:
      readNullary<Uint8NoArgsNode>();
      break;
    case OpUint8OneArg:
      readUint8<Uint8OneArgNode>();
      break;
    case OpUndefine:
      readUnary<UndefineNode>();
      break;
    case OpU8Const:
      readUint8<U8ConstNode>();
      break;
    case OpU32Const:
      readVaruint32<U32ConstNode>();
      break;
    case OpU64Const:
      readVaruint64<U64ConstNode>();
      break;
    case OpVarint32NoArgs:
      readNullary<Varint32NoArgsNode>();
      break;
    case OpVarint32OneArg:
      readUint8<Varint32OneArgNode>();
      break;
    case OpVarint64NoArgs:
      readNullary<Varint64NoArgsNode>();
      break;
    case OpVarint64OneArg:
      readUint8<Varint64OneArgNode>();
      break;
    case OpVaruint32NoArgs:
      readNullary<Varuint32NoArgsNode>();
      break;
    case OpVaruint32OneArg:
      readUint8<Varuint32OneArgNode>();
      break;
    case OpVaruint64NoArgs:
      readNullary<Varuint64NoArgsNode>();
      break;
    case OpVaruint64OneArg:
      readUint8<Varuint64OneArgNode>();
      break;
    case OpVersion:
      readVaruint32<VersionNode>();
      break;
    case OpVoid:
      readNullary<VoidNode>();
      break;
    case OpLastRead:
      readNullary<LastReadNode>();
      break;
    case NO_SUCH_NODETYPE:
    case OpFile:
    case OpSection:
    case OpSymbol:
    case OpUnknownSection:
      TRACE(hex_uint32_t, "Opcode: ", Opcode);
      fatal("Uses construct not implemented yet!");
      break;
  }
}

}  // end of namespace filt

}  // end of namespace wasm
