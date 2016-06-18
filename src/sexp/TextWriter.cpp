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

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

namespace wasm {

namespace filt {

namespace {

constexpr const char *IndentString = "  ";

struct {
  NodeType Type;
  Node::IndexType KidsCountSameLine;
} KidCountData[] = {
  // If not in list, assume 0.
  {NodeType::AppendOneArg, 1},
  {NodeType::Block, 3},
  {NodeType::Case, 2},
  {NodeType::Default, 1},
  {NodeType::Define, 1},
  {NodeType::Eval, 1},
  {NodeType::IfThenElse, 1},
  {NodeType::I32Const, 1},
  {NodeType::I64Const, 1},
  {NodeType::Lit, 1},
  {NodeType::Loop, 1},
  {NodeType::Map, 1},
  {NodeType::Peek, 1},
  {NodeType::Postorder, 1},
  {NodeType::Preorder, 1},
  {NodeType::Read, 1},
  {NodeType::Section, 1},
  {NodeType::Select, 1},
  {NodeType::SymConst, 1},
  {NodeType::Uint32OneArg, 1},
  {NodeType::Uint64OneArg, 1},
  {NodeType::Undefine, 1},
  {NodeType::U32Const, 1},
  {NodeType::U64Const, 1},
  {NodeType::Varint32OneArg, 1},
  {NodeType::Varint64OneArg, 1},
  {NodeType::Varuint32OneArg, 1},
  {NodeType::Varuint64OneArg, 1},
  {NodeType::Version, 1}
};

} // end of anonyous namespace

bool TextWriter::UseNodeTypeNames = false;

TextWriter::TextWriter() {
  for (size_t i = 0; i < NumNodeTypes; ++i) {
    KidCountSameLine.push_back(0);
  }
  for (size_t i = 0; i < size(KidCountData); ++i)
    KidCountSameLine[static_cast<int>(KidCountData[i].Type)]
        = KidCountData[i].KidsCountSameLine;
}

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
  fputs(UseNodeTypeNames ? getNodeTypeName(Type) : getNodeSexpName(Type),
        Writer->File);
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
  LineEmpty = IndentCount == 0;
}

void TextWriter::writeNode(Node *Node, bool AddNewline) {
  if (Node == nullptr) {
    Indent _(this, AddNewline);
    fprintf(File, "<nullptr>");
    LineEmpty = false;
    return;
  }
  switch (NodeType Type = Node->getType()) {
    default: {
      // Write out with number of kids specified to be on same line,
      // with remaining kids on separate (indented) lines.
      Parenthesize _(this, Type, AddNewline);
      Node::IndexType Count = 0;
      Node::IndexType KidsSameLine = KidCountSameLine[static_cast<int>(Type)];
      Node::IndexType NumKids = Node->getNumKids();
      for (auto *Kid : *Node) {
        ++Count;
        if (Count <= KidsSameLine) {
          writeSpace();
          bool AddNewline = Count == KidsSameLine && Count < NumKids;
          writeNode(Kid, AddNewline);
        } else {
          if (Count == 1)
            writeNewline();
          writeNode(Kid, true);
        }
      }
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
      fprintf(File, "%" PRIu64 "", Int->getValue());
      LineEmpty = false;
      return;
    }
    case NodeType::Symbol: {
      Indent _(this, AddNewline);
      auto *Sym = dynamic_cast<SymbolNode*>(Node);
      fprintf(File, "'%s'", Sym->getName().c_str());
      LineEmpty = false;
      return;
    }
  }
}

} // end of namespace filt

} // end of namespace wasm
