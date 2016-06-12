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

/* Implements a textual writer of filter s-expressions */

#include "sexp/TextWriter.h"

namespace wasm {

namespace filt {

namespace {

constexpr const char *IndentString = "  ";

} // end of anonyous namespace

TextWriter::Indent::Indent(TextWriter *Writer, bool AddNewline)
    : Writer(Writer), AddNewline(AddNewline) {
  Writer->writeIndent();
}

TextWriter::Indent::~Indent() {
  Writer->maybeWriteNewline(AddNewline);
}

TextWriter::Parenthesize::Parenthesize(
    TextWriter *Writer, NodeType Type, bool AddNewline)
    : Writer(Writer), AddNewline(AddNewline) {
  Writer->writeIndent();
  fputc('(', Writer->File);
  fputs(getNodeTypeName(Type), Writer->File);
  Writer->LineEmpty = false;
  ++Writer->IndentCount;
}

TextWriter::Parenthesize::~Parenthesize() {
  --Writer->IndentCount;
  Writer->writeIndent();
  fputc(')', Writer->File);
  Writer->LineEmpty = false;
  Writer->maybeWriteNewline(AddNewline);
}

void TextWriter::write(FILE *File, Node *Root) {
  initialize(File);
  writeNode(Root, true);
}

void TextWriter::initialize(FILE *File) {
  this->File = File;
  IndentCount = 0;
  LineEmpty = true;
}

void TextWriter::writeIndent() {
  if (!LineEmpty)
    return;
  for (size_t i = 0; i < IndentCount; ++i)
    fputs(IndentString, File);
  LineEmpty = (IndentCount > 0);
}

void TextWriter::writeNode(Node *Node, bool AddNewline) {
  if (Node == nullptr) {
    Indent _(this, AddNewline);
    fprintf(File, "null");
    LineEmpty = false;
    return;
  }
  switch (NodeType Type = Node->getType()) {
    case NodeType::Append:
    case NodeType::AppendValue:
    case NodeType::Call:
    case NodeType::Copy:
    case NodeType::Eval:
    case NodeType::Fixed32:
    case NodeType::Fixed64:
    case NodeType::I32Const:
    case NodeType::I64Const:
    case NodeType::Lit:
    case NodeType::Map:
    case NodeType::Peek:
    case NodeType::Postorder:
    case NodeType::Preorder:
    case NodeType::Read:
    case NodeType::SymConst:
    case NodeType::Uint8:
    case NodeType::Uint32:
    case NodeType::Uint64:
    case NodeType::U32Const:
    case NodeType::U64Const:
    case NodeType::Value:
    case NodeType::Varint32:
    case NodeType::Varint64:
    case NodeType::Varuint1:
    case NodeType::Varuint7:
    case NodeType::Varuint32:
    case NodeType::Varuint64:
    case NodeType::Vbrint32:
    case NodeType::Vbrint64:
    case NodeType::Vbruint32:
    case NodeType::Vbruint64:
    case NodeType::Version:
    case NodeType::Void:
    case NodeType::Write: {
      // Treat like expression, which goes on same line.
      Parenthesize _(this, Type, AddNewline);
      for (auto *Kid : *Node) {
        writeSpace();
        writeNode(Kid, false);
      }
      return;
    }
    case NodeType::Case:
    case NodeType::Extract: {
      // Treat like statement where all but first element put on separate lines,
      // unless there is only 2 or less arguments.
      Parenthesize _(this, Type, AddNewline);
      bool KidAddNewline = Node->getNumKids() > 2;
      for (auto *Kid : *Node) {
        if (!KidAddNewline)
          writeSpace();
        writeNode(Kid, KidAddNewline);
      }
      return;
    }
    case NodeType::Define:
    case NodeType::IfThenElse:
    case NodeType::Loop:
    case NodeType::Method:
    case NodeType::Section:
    case NodeType::Select: {
      // Treat like statement where all but first element put on separate lines.
      Parenthesize _(this, Type, AddNewline);
      bool IsFirst = true;
      for (auto *Kid : *Node) {
        if (IsFirst) {
          writeSpace();
          IsFirst = false;
        }
        writeNode(Kid, true);
      }
      return;
    }
    case NodeType::AstToBit:
    case NodeType::AstToByte:
    case NodeType::AstToInt:
    case NodeType::BitToAst:
    case NodeType::BitToBit:
    case NodeType::BitToByte:
    case NodeType::BitToInt:
    case NodeType::ByteToAst:
    case NodeType::ByteToBit:
    case NodeType::ByteToByte:
    case NodeType::ByteToInt:
    case NodeType::Filter:
    case NodeType::IntToAst:
    case NodeType::IntToBit:
    case NodeType::IntToByte:
    case NodeType::IntToInt:
    case NodeType::LoopUnbounded:
    case NodeType::Sequence: {
      // Treat like statement where all elements put on separate line.
      Parenthesize _(this, Type, AddNewline);
      if (Node->getNumKids() == 0)
        return;
      writeNewline();
      for (auto *Kid : *Node)
        writeNode(Kid, true);
      return;
    }
    case NodeType::File: {
      // Treat like hidden node. That is, visually just a list of s-expressions.
      for (auto *Kid : *Node)
        writeNode(Kid, true);
      return;
    }
    case NodeType::Integer: {
      Indent _(this, AddNewline);
      auto *Int = dynamic_cast<IntegerNode*>(Node);
      // TODO: Get sign/format correct.
      // TODO: Get format directive correct on all platforms!
      fprintf(File, "%lu", Int->getValue());
      LineEmpty = false;
      return;
    }
    case NodeType::Symbol: {
      Indent _(this, AddNewline);
      auto *Sym = dynamic_cast<SymbolNode*>(Node);
      // TODO: Get if needs to be quoted.
      fprintf(File, "%s", Sym->getName().c_str());
      LineEmpty = false;
      return;
    }
  }
}

} // end of namespace filt

} // end of namespace wasm
