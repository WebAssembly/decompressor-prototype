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

//#include <cctype>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sexp/Ast.h"
#include "stream/WriteUtils.h"
#include "utils/Casting.h"

namespace wasm {

using namespace utils;

namespace filt {

namespace {
constexpr const char* IndentString = "  ";
}  // end of anonyous namespace

bool TextWriter::DefaultShowInternalStructure = false;

TextWriter::TextWriter()
    : File(nullptr),
      IndentCount(0),
      LineEmpty(true),
      ShowInternalStructure(DefaultShowInternalStructure) {}

TextWriter::Indent::Indent(TextWriter* Writer, bool AddNewline)
    : Writer(Writer), AddNewline(AddNewline) {
  Writer->writeIndent();
}

TextWriter::Indent::~Indent() {
  Writer->maybeWriteNewline(AddNewline);
}

TextWriter::Parenthesize::Parenthesize(TextWriter* Writer,
                                       NodeType Type,
                                       bool AddNewline)
    : Writer(Writer), AddNewline(AddNewline) {
  Writer->writeIndent();
  fputc('(', Writer->File);
  Writer->writeName(Type);
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

charstring TextWriter::getNodeName(NodeType Type) const {
  return ShowInternalStructure ? getNodeTypeName(Type) : getNodeSexpName(Type);
}

charstring TextWriter::getNodeName(const Node* Nd) const {
  return getNodeName(Nd->getType());
}

void TextWriter::writeName(NodeType Type) {
  fputs(getNodeName(Type), File);
}

void TextWriter::write(FILE* File, SymbolTable* Symtab) {
  if (Symtab == nullptr) {
    fprintf(File, "nullptr\n");
    return;
  }
  write(File, Symtab->getAlgorithm());
  Symtab = Symtab->getEnclosingScope().get();
  while (Symtab != nullptr) {
    writeIndent();
    fprintf(File, "Enclosing scope:\n");
    write(File, Symtab->getAlgorithm());
    Symtab = Symtab->getEnclosingScope().get();
  }
}

void TextWriter::write(FILE* File, const Node* Root) {
  initialize(File);
  writeNode(Root, true);
}

void TextWriter::writeAbbrev(FILE* File, const Node* Root) {
  initialize(File);
  writeNodeAbbrev(Root, true);
}

void TextWriter::initialize(FILE* File) {
  this->File = File;
  IndentCount = 0;
  LineEmpty = true;
}

void TextWriter::writeIndent(int Adjustment) {
  if (!LineEmpty)
    return;
  size_t Limit = IndentCount + Adjustment;
  for (size_t i = 0; i < Limit; ++i)
    fputs(IndentString, File);
  LineEmpty = IndentCount == 0;
}

void TextWriter::writeNodeKids(const Node* Nd, bool EmbeddedInParent) {
  // Write out with number of kids specified to be on same line,
  // with remaining kids on separate (indented) lines.
  int Count = 0;
  const AstTraitsType* Traits = getAstTraits(Nd->getType());
  int KidsSameLine = Traits->NumTextArgs;
  int MaxKidsSameLine = KidsSameLine + Traits->AdditionalTextArgs;
  int NumKids = Nd->getNumKids();
  if (NumKids <= MaxKidsSameLine)
    KidsSameLine = MaxKidsSameLine;
  Node* LastKid = Nd->getLastKid();
  bool HasHiddenSeq = Traits->HidesSeqInText;
  bool ForceNewline = false;
  for (auto* Kid : *Nd) {
    if (Kid == LastKid && !ShowInternalStructure) {
      bool IsEmbedded = (HasHiddenSeq && isa<Sequence>(LastKid)) ||
                        (isa<Case>(Nd) && isa<Case>(Kid));
      if (IsEmbedded) {
        writeNewline();
        writeNode(Kid, true, true);
        return;
      }
    }
    ++Count;
    if (ForceNewline) {
      writeNode(Kid, true);
      continue;
    }
    if (getAstTraits(Kid->getType())->NeverSameLineInText) {
      if (!(Count == 1 && EmbeddedInParent))
        writeNewline();
      ForceNewline = true;
      writeNode(Kid, true);
      continue;
    }
    if (Count < KidsSameLine) {
      writeSpace();
      writeNode(Kid, false);
      continue;
    }
    if (Count == KidsSameLine) {
      writeSpace();
      ForceNewline = Count < NumKids;
      writeNode(Kid, ForceNewline);
      continue;
    }
    if (Count > KidsSameLine)
      writeNewline();
    ForceNewline = true;
    writeNode(Kid, true);
  }
}

void TextWriter::writeNode(const Node* Nd,
                           bool AddNewline,
                           bool EmbedInParent) {
  if (Nd == nullptr) {
    Indent _(this, AddNewline);
    fprintf(File, "null");
    LineEmpty = false;
    maybeWriteNewline(AddNewline);
    return;
  }
  NodeType Type = Nd->getType();
  if (isa<IntegerNode>(Nd))
    return writeIntegerNode(cast<IntegerNode>(Nd), AddNewline);
  switch (Type) {
    default: {
      if (!(EmbedInParent && !ShowInternalStructure))
        break;
      if (isa<Case>(Nd)) {
        writeIndent(-1);
        writeSpace();
        writeName(NodeType::Case);
      }
      writeNodeKids(Nd, true);
      return;
    }
    case NodeType::Algorithm: {
      if (ShowInternalStructure)
        break;
      // Treat like hidden node. That is, visually just a list of
      // s-expressions.
      for (auto* Kid : *Nd)
        writeNode(Kid, true);
      return;
    }
    case NodeType::LiteralUse:
    case NodeType::LiteralActionUse:
      if (ShowInternalStructure)
        break;
      writeNode(Nd->getKid(0), AddNewline, EmbedInParent);
      return;
    case NodeType::Symbol:
      return writeSymbol(cast<Symbol>(Nd), AddNewline);
    case NodeType::SymbolDefn: {
      Parenthesize _(this, Type, AddNewline);
      writeSpace();
      writeNode(cast<SymbolDefn>(Nd)->getSymbol(), false);
      return;
    }
  }
  // Default case.
  Parenthesize _(this, Type, AddNewline);
  writeNodeKids(Nd, false);
}

void TextWriter::writeNodeKidsAbbrev(const Node* Nd, bool EmbeddedInParent) {
  // Write out with number of kids specified to be on same line,
  // with remaining kids on separate (indented) lines.

  const AstTraitsType* Traits = getAstTraits(Nd->getType());
  int KidsSameLine = Traits->NumTextArgs;
  int MaxKidsSameLine = KidsSameLine + Traits->AdditionalTextArgs;
  int NumKids = Nd->getNumKids();
  if (NumKids <= MaxKidsSameLine)
    KidsSameLine = MaxKidsSameLine;
  bool HasHiddenSeq = Traits->HidesSeqInText;
  for (int i = 0; i < NumKids; ++i) {
    Node* Kid = Nd->getKid(i);
    bool LastKid = i + 1 == NumKids;
    if (HasHiddenSeq && LastKid && isa<Sequence>(Kid)) {
      fprintf(File, " ...[%d]", Kid->getNumKids());
      return;
    }
    if (getAstTraits(Kid->getType())->NeverSameLineInText) {
      fprintf(File, " ...[%d]", NumKids - i);
      return;
    }
    int Count = i + 1;
    if (Count < KidsSameLine) {
      writeSpace();
      writeNodeAbbrev(Kid, false);
      continue;
    }
    if (Count == KidsSameLine) {
      writeSpace();
      writeNodeAbbrev(Kid, false);
      if (!LastKid)
        fprintf(File, " ...[%d]", NumKids - Count);
      return;
    }
    if (Count == 1 && EmbeddedInParent) {
      fprintf(File, " ...[%d]", NumKids);
      return;
    }
    writeSpace();
    writeNodeAbbrev(Kid, false);
    if (!LastKid)
      fprintf(File, " ...[%d]", NumKids - Count);
    return;
  }
}

void TextWriter::writeNodeAbbrev(const Node* Nd,
                                 bool AddNewline,
                                 bool EmbedInParent) {
  if (Nd == nullptr) {
    fprintf(File, "null");
    LineEmpty = false;
    maybeWriteNewline(AddNewline);
    return;
  }
  if (isa<IntegerNode>(Nd))
    return writeIntegerNode(cast<IntegerNode>(Nd), AddNewline);
  NodeType Type = Nd->getType();
  switch (Type) {
    default: {
      if (!EmbedInParent)
        break;
      fprintf(File, " ...");
      return;
    }
    case NodeType::Algorithm: {
      // Treat like hidden node. That is, visually just a list of s-expressions.
      fprintf(File, "(%s ...)\n", getNodeName(Nd));
      return;
    }
    case NodeType::LiteralUse:
    case NodeType::LiteralActionUse:
      if (ShowInternalStructure)
        break;
      writeNodeAbbrev(Nd->getKid(0), AddNewline, EmbedInParent);
      return;
    case NodeType::Symbol:
      return writeSymbol(cast<Symbol>(Nd), AddNewline);
    case NodeType::SymbolDefn:
      writeNode(Nd, AddNewline, EmbedInParent);
      return;
  }
  Parenthesize _(this, Type, AddNewline);
  writeNodeKidsAbbrev(Nd, false);
}

void TextWriter::writeSymbolName(std::string Name) {
  fputc('\'', File);
  for (char V : Name) {
    switch (V) {
      case '\\':
        fputc('\\', File);
        fputc('\\', File);
        continue;
      case '\f':
        fputc('\\', File);
        fputc('f', File);
        continue;
      case '\n':
        fputc('\\', File);
        fputc('n', File);
        continue;
      case '\r':
        fputc('\\', File);
        fputc('r', File);
        continue;
      case '\t':
        fputc('\\', File);
        fputc('t', File);
        continue;
      case '\v':
        fputc('\\', File);
        fputc('v', File);
        continue;
    }
    if (isprint(V)) {
      fputc(V, File);
      continue;
    }
    fputc('\\', File);
    uint8_t BitsPerOctal = 3;
    uint8_t Shift = 2 * BitsPerOctal;
    for (int Count = 3; Count > 0; --Count) {
      uint8_t Digit = V >> Shift;
      fputc('0' + Digit, File);
      V &= ((1 << Shift) - 1);
      Shift -= BitsPerOctal;
    }
  }
  fputc('\'', File);
}

void TextWriter::writeSymbol(const Symbol* Sym, bool AddNewline) {
  if (ShowInternalStructure) {
    Parenthesize _(this, Sym->getType(), AddNewline);
    writeSpace();
    writeSymbolName(Sym->getName());
  } else {
    Indent _(this, AddNewline);
    writeSymbolName(Sym->getName());
    LineEmpty = false;
  }
  return;
}

void TextWriter::writeIntegerNode(const IntegerNode* Int, bool AddNewline) {
  Parenthesize _(this, Int->getType(), AddNewline);
  if (!Int->isDefaultValue()) {
    fputc(' ', File);
    writeInt(File, Int->getValue(), Int->getFormat());
    if (const auto* Accept = dyn_cast<BinaryAccept>(Int)) {
      fputc(':', File);
      fprintf(File, "%u", Accept->getNumBits());
    }
  }
  LineEmpty = false;
  return;
}

void TextWriter::writeNewline() {
  if (!LineEmpty)
    fputc('\n', File);
  LineEmpty = true;
}

void TextWriter::maybeWriteNewline(bool Yes) {
  if (Yes)
    writeNewline();
}

void TextWriter::writeSpace() {
  fputc(' ', File);
  LineEmpty = false;
}

}  // end of namespace filt

}  // end of namespace wasm
