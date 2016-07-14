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

BinaryReader::BinaryReader(decode::ByteQueue *Input, alloc::Allocator *Alloc) :
    Alloc(Alloc), ReadPos(Input), SectionSymtab(Alloc) {
  Reader = Alloc->create<ByteReadStream>();
}

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
    case OpAnd: {
      if (NodeStack.size() < 2)
        fatal("Can't find arguments for and s-expression");
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Test = Alloc->create<AndNode>(Arg1, Arg2);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Test);
      }
      NodeStack.push_back(Test);
      break;
    }
    case OpBlock: {
      if (NodeStack.size() < 1)
        fatal("Can't find arguments for block s-expression");
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Block = Alloc->create<BlockNode>(Arg1);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Block);
      }
      NodeStack.push_back(Block);
      break;
    }
    case OpBlockEndNoArgs: {
      auto *End = Alloc->create<BlockEndNoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, End);
      }
      NodeStack.push_back(End);
      break;
    }
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
    case OpDefault: {
      auto *Symbol =
          SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      auto *Body = NodeStack.back();
      NodeStack.pop_back();
      auto *Default = Alloc->create<DefaultNode>(Symbol, Body);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Default);
      }
      NodeStack.push_back(Default);
      break;
    }
    case OpDefine: {
      auto *Symbol =
          SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      auto *Body = NodeStack.back();
      NodeStack.pop_back();
      auto *Define = Alloc->create<DefineNode>(Symbol, Body);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Define);
      }
      NodeStack.push_back(Define);
      break;
    }
    case OpEval: {
      SymbolNode *Symbol =
          SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      auto *Eval = Alloc->create<EvalNode>(Symbol);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Eval);
      }
      NodeStack.push_back(Eval);
      break;
    };
    case OpEvalDefault: {
      SymbolNode *Symbol =
          SectionSymtab.getIndexSymbol(Reader->readVaruint32(ReadPos));
      auto *Eval = Alloc->create<EvalDefaultNode>(Symbol);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Eval);
      }
      NodeStack.push_back(Eval);
      break;
    };
    case OpIfThen: {
      if (NodeStack.size() < 2)
        fatal("Can't find arguments for if-then s-expression");
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *If = Alloc->create<IfThenNode>(Arg1, Arg2);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, If);
      }
      NodeStack.push_back(If);
      break;
    }
    case OpIfThenElse: {
      if (NodeStack.size() < 3)
        fatal("Can't find arguments for if-then-else s-expression");
      Node *Arg3 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *If = Alloc->create<IfThenElseNode>(Arg1, Arg2, Arg3);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, If);
      }
      NodeStack.push_back(If);
      break;
    }
    case OpIsByteIn: {
      auto *Test = Alloc->create<IsByteInNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Test);
      }
      NodeStack.push_back(Test);
      break;
    }
    case OpIsByteOut: {
      auto *Test = Alloc->create<IsByteOutNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Test);
      }
      NodeStack.push_back(Test);
      break;
    }
    case OpI32Const: {
      auto *Number =
          Alloc->create<IntegerNode>(Reader->readVarint32(ReadPos));
      auto *Constant = Alloc->create<I32ConstNode>(Number);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Constant);
      }
      NodeStack.push_back(Constant);
      break;
    }
    case OpI64Const: {
      auto *Number =
          Alloc->create<IntegerNode>(Reader->readVarint64(ReadPos));
      auto *Constant = Alloc->create<I64ConstNode>(Number);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Constant);
      }
      NodeStack.push_back(Constant);
      break;
    }
    case OpLoop: {
      if (NodeStack.size() < 2)
        fatal("Can't find arguments for loop s-expression");
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Loop = Alloc->create<LoopNode>(Arg1, Arg2);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Loop);
      }
      NodeStack.push_back(Loop);
      break;
    }
    case OpLoopUnbounded: {
      if (NodeStack.size() < 1)
        fatal("Can't find arguments for unbounded loop s-expression");
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Loop = Alloc->create<LoopUnboundedNode>(Arg1);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Loop);
      }
      NodeStack.push_back(Loop);
      break;
    }
    case OpOr: {
      if (NodeStack.size() < 2)
        fatal("Can't find arguments for or s-expression");
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Test = Alloc->create<OrNode>(Arg1, Arg2);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Test);
      }
      NodeStack.push_back(Test);
      break;
    }
    case OpPeek: {
      if (NodeStack.size() < 1)
        fatal("Can't find arguments for peek s-expression");
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Peek = Alloc->create<PeekNode>(Arg1);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Peek);
      }
      NodeStack.push_back(Peek);
      break;
    }
    case OpNot: {
      if (NodeStack.size() < 1)
        fatal("Can't find arguments for not s-expression");
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Test = Alloc->create<NotNode>(Arg1);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Test);
      }
      NodeStack.push_back(Test);
      break;
    }
    case OpMap: {
      if (NodeStack.size() < 2)
        fatal("Can't find arguments for map s-expression");
      Node *Arg2 = NodeStack.back();
      NodeStack.pop_back();
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Expression = Alloc->create<MapNode>(Arg1, Arg2);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Expression);
      }
      NodeStack.push_back(Expression);
      break;
    }
    case OpRead: {
      if (NodeStack.size() < 1)
        fatal("Can't find arguments for read s-expression");
      Node *Arg1 = NodeStack.back();
      NodeStack.pop_back();
      auto *Read = Alloc->create<ReadNode>(Arg1);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Read);
      }
      NodeStack.push_back(Read);
      break;
    }
    case OpSelect: {
      uint32_t NumKids = Reader->readVaruint32(ReadPos);
      size_t StackSize = NodeStack.size();
      if (StackSize < NumKids)
        fatal("Can't find arguments for select s-expression");
      auto *Select = Alloc->create<SelectNode>();
      for (size_t i = StackSize - NumKids; i < StackSize; ++i)
        Select->append(NodeStack[i]);
      for (uint32_t i = 0; i < NumKids; ++i)
        NodeStack.pop_back();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Select);
      }
      NodeStack.push_back(Select);
      break;
    }
    case OpSequence: {
      uint32_t NumKids = Reader->readVaruint32(ReadPos);
      size_t StackSize = NodeStack.size();
      if (StackSize < NumKids)
        fatal("Can't find arguments for sequence s-expression");
      auto *Seq = Alloc->create<SequenceNode>();
      for (size_t i = StackSize - NumKids; i < StackSize; ++i)
        Seq->append(NodeStack[i]);
      for (uint32_t i = 0; i < NumKids; ++i)
        NodeStack.pop_back();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Seq);
      }
      NodeStack.push_back(Seq);
      break;
    }
    case OpUint32NoArgs: {
      auto *Format = Alloc->create<Uint8NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpUint32OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Uint32OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpUint64NoArgs: {
      auto *Format = Alloc->create<Uint64NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpUint64OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Uint64OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpUint8NoArgs: {
      auto *Format = Alloc->create<Uint8NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpUint8OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Uint8OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpU32Const: {
      auto *Number =
          Alloc->create<IntegerNode>(Reader->readVaruint32(ReadPos));
      auto *Constant = Alloc->create<U32ConstNode>(Number);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Constant);
      }
      NodeStack.push_back(Constant);
      break;
    }
    case OpU64Const: {
      auto *Number =
          Alloc->create<IntegerNode>(Reader->readVaruint64(ReadPos));
      auto *Constant = Alloc->create<U64ConstNode>(Number);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Constant);
      }
      NodeStack.push_back(Constant);
      break;
    }
    case OpVarint32NoArgs: {
      auto *Format = Alloc->create<Varint32NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVarint32OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Varint32OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVarint64NoArgs: {
      auto *Format = Alloc->create<Varint64NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVarint64OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Varint64OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVaruint32NoArgs: {
      auto *Format = Alloc->create<Varuint32NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVaruint32OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Varuint32OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVaruint64NoArgs: {
      auto *Format = Alloc->create<Varuint64NoArgsNode>();
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVaruint64OneArg: {
      auto *BitCount =
          Alloc->create<IntegerNode>(Reader->readUint8(ReadPos));
      auto *Format = Alloc->create<Varuint64OneArgNode>(BitCount);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Format);
      }
      NodeStack.push_back(Format);
      break;
    }
    case OpVersion: {
      auto *Number =
          Alloc->create<IntegerNode>(Reader->readVaruint32(ReadPos));
      auto *Version = Alloc->create<VersionNode>(Number);
      if (TraceProgress) {
        writeIndent();
        getTraceWriter()->writeAbbrev(stderr, Version);
      }
      NodeStack.push_back(Version);
      break;
    }
    case OpVoid:
      NodeStack.push_back(Alloc->create<VoidNode>());
      break;
    case OpFilter:
    case OpInteger:
    case OpError:
    case OpByteToByte:
    case OpFile:
    case OpSection:
    case OpSymbol:
    case OpUndefine:
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
