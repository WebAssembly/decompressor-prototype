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

using namespace wasm::decode;

namespace wasm {

namespace filt {

TextWriter *BinaryReader::getTraceWriter() {
  if (TraceWriter == nullptr)
    TraceWriter = new TextWriter();
  return TraceWriter;
}

void BinaryReader::writeReadPos(FILE *File) {
  fprintf(stderr, "@%" PRIuMAX " ", intmax_t(ReadPos.getCurAddress()));
}

void BinaryReader::writeIndent() {
  for (int i = 0; i < IndentLevel; ++i)
    fprintf(stderr, "  ");
  writeReadPos(stderr);
}

template<class Type>
void BinaryReader::returnValueInternal(const char *Name, Type Value) {
  IndentEnd();
  fprintf(stderr, "<- %s = %" PRIuMAX"\n", Name, intmax_t(Value));

}

template<> void BinaryReader::returnValueInternal<const Node *>(
    const char *Name, const Node *Value) {
  IndentEnd();
  fprintf(stderr, "<- %s = %p\n", Name, (void*)(Value));
}


void BinaryReader::enterInternal(const char *Name, bool AddNewline) {
  IndentBegin();
  fprintf(stderr, "-> %s", Name);
  if (AddNewline)
    fputc('\n', stderr);
  else
    fputc(' ', stderr);
}

void BinaryReader::exitInternal(const char *Name) {
  IndentEnd();
  fprintf(stderr, "<- %s\n", Name);
}

BinaryReader::BinaryReader(decode::ByteQueue *Input, alloc::Allocator *Alloc) :
    Alloc(Alloc), ReadPos(Input), SectionSymtab(Alloc) {
  Reader = Alloc->create<ByteReadStream>();
}

template<class T>
void BinaryReader::readNullary() {
  auto *Node = Alloc->create<T>();
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnary() {
  if (NodeStack.size() < 1)
    fatal("Can't find arguments for s-expression");
  Node *Arg = NodeStack.back();
  NodeStack.pop_back();
  auto *Node = Alloc->create<T>(Arg);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnarySymbol() {
  auto *Symbol =
      SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto *Node = Alloc->create<T>(Symbol);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnaryUint8() {
  auto *Number = Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
  auto *Node = Alloc->create<T>(Number);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnaryVarint32() {
  auto *Number = Alloc->create<IntegerNode>(Reader->readVarint32(ReadPos));
  auto *Node = Alloc->create<T>(Number);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}


template<class T>
void BinaryReader::readUnaryVarint64() {
  auto *Number = Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
  auto *Node = Alloc->create<T>(Number);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnaryVaruint32() {
  auto *Number = Alloc->create<IntegerNode>(Reader->readVaruint32(ReadPos));
  auto *Node = Alloc->create<T>(Number);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readUnaryVaruint64() {
  auto *Number =
      Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
  auto *Node = Alloc->create<T>(Number);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readBinary() {
  if (NodeStack.size() < 2)
    fatal("Can't find arguments for s-expression");
  Node *Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node *Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto *Node = Alloc->create<T>(Arg1, Arg2);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readBinarySymbol() {
  auto *Symbol =
      SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
  auto *Body = NodeStack.back();
  NodeStack.pop_back();
  auto *Node = Alloc->create<T>(Symbol, Body);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readTernary() {
  if (NodeStack.size() < 3)
    fatal("Can't find arguments for s-expression");
  Node *Arg3 = NodeStack.back();
  NodeStack.pop_back();
  Node *Arg2 = NodeStack.back();
  NodeStack.pop_back();
  Node *Arg1 = NodeStack.back();
  NodeStack.pop_back();
  auto *Node = Alloc->create<T>(Arg1, Arg2, Arg3);
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

template<class T>
void BinaryReader::readNary() {
  uint32_t NumKids = Reader->readVaruint32(ReadPos);
  size_t StackSize = NodeStack.size();
  if (StackSize < NumKids)
    fatal("Can't find arguments for s-expression");
  auto *Node = Alloc->create<T>();
  for (size_t i = StackSize - NumKids; i < StackSize; ++i)
    Node->append(NodeStack[i]);
  for (uint32_t i = 0; i < NumKids; ++i)
    NodeStack.pop_back();
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Node);
  }
  NodeStack.push_back(Node);
}

FileNode *BinaryReader::readFile() {
  enter("readFile");
  MagicNumber = Reader->readUint32(ReadPos);
  // TODO(kschimpf): Fix reading of uintX. Current implementation not same as
  // WASM binary reader.
  if (TraceProgress) {
    writeIndent();
    fprintf(stderr, "MagicNumber = %x\n", MagicNumber);
  }
  if (MagicNumber != WasmBinaryMagic)
    fatal("Unable to read, did not find WASM binary magic number");
  Version = Reader->readUint32(ReadPos);
  if (TraceProgress) {
    writeIndent();
    fprintf(stderr, "Version = %x\n", Version);
  }
  auto *File = Alloc->create<FileNode>();
  while (!ReadPos.atEob())
    File->append(readSection());
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, File);
  }
  exit("readFile");
  return File;
}

SectionNode *BinaryReader::readSection() {
  enter("readSection");
  ExternalName Name = readExternalName();
  auto *SectionName = Alloc->create<SymbolNode>(Name);
  auto *Section = Alloc->create<SectionNode>();
  Section->append(SectionName);
  if (TraceProgress) {
    writeIndent();
    fprintf(stderr, "Name = %s\n", Name.c_str());
  }
  size_t StartStackSize = NodeStack.size();
  readBlock(
      [&]() {
        if (Name == "filter") {
          readSymbolTable();
          while (!ReadPos.atEob())
            readNode();
        } else {
          // TODO(karlschimpf) Fix to actually read!
          writeReadPos(stderr);
          fprintf(stderr, "Skipping unknown section\n");
        }
      });
  size_t StackSize = NodeStack.size();
  if (StackSize < StartStackSize)
    fatal("Malformed section: " + Name);
  for (size_t i = StartStackSize; i < StackSize; ++i)
    Section->append(NodeStack[i]);
  for (size_t i = StartStackSize; i < StackSize; ++i)
    NodeStack.pop_back();
  if (TraceProgress) {
    writeIndent();
    getTraceWriter()->writeAbbrev(stderr, Section);
  }
  exit("readSection");
  return Section;
}

void BinaryReader::readSymbolTable() {
  enter("readSymbolTable");
  SectionSymtab.clear();
  uint32_t NumSymbols = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NumSymbols; ++i) {
    ExternalName &Name = readExternalName();
    if (TraceProgress) {
      writeIndent();
      fprintf(stderr, "Symbol %" PRIuMAX ": %s\n",
              intmax_t(i), Name.c_str());
    }
    SectionSymtab.addSymbol(Name);
  }
  exit("readSymbolTable");
}

ExternalName &BinaryReader::readExternalName() {
  static ExternalName Name;
  Name.clear();
  uint32_t NameSize = Reader->readVaruint32(ReadPos);
  for (uint32_t i = 0; i < NameSize; ++i) {
    uint8_t Byte = Reader->readUint8(ReadPos);
    Name.push_back(char(Byte));
  }
  return Name;
}

InternalName &BinaryReader::readInternalName() {
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
  enter("readBlock");
  const size_t BlockSize = Reader->readVaruint32(ReadPos);
  if (TraceProgress) {
    writeIndent();
    fprintf(stderr, "block size: %" PRIuMAX "\n", intmax_t(BlockSize));
  }
  ReadPos.pushEobAddress(ReadPos.getCurAddress() + BlockSize);
  ApplyFn();
  ReadPos.popEobAddress();
  exit("readBlock");
}

void BinaryReader::readNode() {
  enter("readNode");
  NodeType Opcode = (NodeType)Reader->readUint8(ReadPos);
  switch(Opcode) {
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
      auto *Key =
          Alloc->create<IntegerNode>(Reader->readVarint32(ReadPos));
      Node *Code = NodeStack.back();
      NodeStack.pop_back();
      auto *Case = Alloc->create<CaseNode>(Key, Code);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Case);
      }
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
    case OpSelect:
      readNary<SelectNode>();
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
    case OpInteger:
    case OpFile:
    case OpSection:
    case OpSymbol:
    case OpUnknownSection:
      if (TraceProgress) {
        writeIndent();
        fprintf(stderr, "Unimplemented opcode = %x\n", Opcode);
      }
      fatal("Uses construct not implemented yet!");
      break;
  }
  exit("readNode");
}

} // end of namespace filt

} // end of namespace wasm
