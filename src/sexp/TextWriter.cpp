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
#include "stream/WriteUtils.h"

#include <cctype>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

namespace wasm {

namespace filt {

namespace {

decode::IntType divideByPower10(decode::IntType Value,
                                decode::IntType Power10) {
  if (Power10 <= 1)
    return Value;
  return Value / Power10;
}

decode::IntType moduloByPower10(decode::IntType Value,
                                decode::IntType Power10) {
  if (Power10 <= 1)
    return Value;
  return Value % Power10;
}

#if 0
char getHexCharForDigit(uint8_t Digit) {
  return Digit < 10 ? '0' + Digit : 'a' + (Digit - 10);
}
#endif

constexpr const char *IndentString = "  ";

} // end of anonyous namespace

bool TextWriter::UseNodeTypeNames = false;

TextWriter::TextWriter() {
  // Build fast lookup for number of arguments to write on same line.
  for (size_t i = 0; i < MaxNodeType; ++i) {
    KidCountSameLine.push_back(0);
    MaxKidCountSameLine.push_back(0);
  }
  for (size_t i = 0; i < NumNodeTypes; ++i) {
    AstTraitsType &Traits = AstTraits[i];
    KidCountSameLine[int(Traits.Type)] = Traits.NumTextArgs;
    MaxKidCountSameLine[int(Traits.Type)] =
        Traits.NumTextArgs + Traits.AdditionalTextArgs;
  }
  // Build map of nodes that can have hidden seq as last kid.
#define X(tag) \
  HasHiddenSeqSet.insert(int(Op##tag));
  AST_NODE_HAS_HIDDEN_SEQ
#undef X
  // Build map of nodes that never should be on the same line
  // as its parent.
#define X(tag) \
  NeverSameLine.insert(int(Op##tag));
  AST_NODE_NEVER_SAME_LINE
#undef X
  // Compute that maximum power of 10 that can still be an IntType.
  decode::IntType MaxPower10 = 1;
  decode::IntType NextMaxPower10 = MaxPower10 * 10;
  while (NextMaxPower10 > MaxPower10) {
    MaxPower10 = NextMaxPower10;
    NextMaxPower10 *= 10;
  }
  IntTypeMaxPower10 = MaxPower10;
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

void TextWriter::write(FILE *File, const Node *Root) {
  initialize(File);
  writeNode(Root, true);
}

void TextWriter::writeAbbrev(FILE *File, const Node *Root) {
  initialize(File);
  writeNodeAbbrev(Root, true);
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

void TextWriter::writeNodeKids(const Node *Nd, bool EmbeddedInParent) {
  // Write out with number of kids specified to be on same line,
  // with remaining kids on separate (indented) lines.
  int Type = int(Nd->getType());
  int Count = 0;
  int KidsSameLine = KidCountSameLine[int(Type)];
  int NumKids = Nd->getNumKids();
  if (NumKids <= MaxKidCountSameLine[int(Type)])
    KidsSameLine = MaxKidCountSameLine[int(Type)];
  Node *LastKid = Nd->getLastKid();
  int HasHiddenSeq = HasHiddenSeqSet.count(Type);
  bool ForceNewline = false;
  for (auto *Kid : *Nd) {
    if (HasHiddenSeq && Kid == LastKid && isa<SequenceNode>(LastKid)) {
      writeNode(Kid, true, HasHiddenSeq);
      return;
    }
    ++Count;
    if (ForceNewline) {
      writeNode(Kid, true);
      continue;
    }
    if (NeverSameLine.count(Kid->getType())) {
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
    if (Count == 1 && EmbeddedInParent)
      writeNewline();
    ForceNewline = true;
    writeNode(Kid, true);
  }
}

void TextWriter::writeNode(const Node *Nd, bool AddNewline,
                           bool EmbedInParent) {
  switch (NodeType Type = Nd->getType()) {
    default: {
      if (EmbedInParent) {
        writeNodeKids(Nd, true);
      } else {
        Parenthesize _(this, Nd->getType(), AddNewline);
        writeNodeKids(Nd, false);
      }
      return;
    }
    case OpFile: {
      // Treat like hidden node. That is, visually just a list of s-expressions.
      for (auto *Kid : *Nd)
        writeNode(Kid, true);
      return;
    }
    case OpInteger: {
      Indent _(this, AddNewline);
      const auto *Int = cast<IntegerNode>(Nd);
      decode::IntType Value = Int->getValue();
      switch (Int->getFormat()) {
        case ValueFormat::SignedDecimal: {
          decode::SignedIntType SignedValue = decode::SignedIntType(Value);
          if (SignedValue < 0) {
            fputc('-', File);
            Value = decode::IntType(-SignedValue);
          }
        }
        // Intentionally fall to next case.
        case ValueFormat::Decimal: {
          decode::IntType Power10 = IntTypeMaxPower10;
          bool StartPrinting = false;
          while (Power10 > 0) {
            decode::IntType Digit = divideByPower10(Value, Power10);
            if (StartPrinting || Digit) {
                if (StartPrinting || Digit != 0) {
                  StartPrinting = true;
                  fputc('0' + Digit, File);
                }
            }
            Value = moduloByPower10(Value, Power10);
            Power10 /= 10;
          }
          if (!StartPrinting)
            fputc('0', File);
          break;
        }
        case ValueFormat::Hexidecimal: {
          constexpr decode::IntType BitsInHex = 4;
          decode::IntType Shift = sizeof(decode::IntType) * CHAR_BIT;
          bool StartPrinting = false;
          fputc('0', File);
          fputc('x', File);
          while (Shift > 0) {
            Shift >>= BitsInHex;
            decode::IntType Digit = (Value >> Shift);
            if (StartPrinting || Digit != 0) {
              StartPrinting = true;
              fputc(getHexCharForDigit(Digit), File);
              Value &= (1 << Shift) - 1;
            }
          }
          break;
        }
      }
      LineEmpty = false;
      return;
    }
    case OpSymbol: {
      Indent _(this, AddNewline);
      const auto *Sym = cast<SymbolNode>(Nd);
      fputc('\'', File);
      for (uint8_t V : Sym->getName()) {
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
      LineEmpty = false;
      return;
    }
  }
}

void TextWriter::writeNodeKidsAbbrev(const Node *Nd, bool EmbeddedInParent) {
  // Write out with number of kids specified to be on same line,
  // with remaining kids on separate (indented) lines.
  int Type = int(Nd->getType());
  int Count = 0;
  int KidsSameLine = KidCountSameLine[int(Type)];
  int NumKids = Nd->getNumKids();
  if (NumKids <= MaxKidCountSameLine[int(Type)])
    KidsSameLine = MaxKidCountSameLine[int(Type)];
  Node *LastKid = Nd->getLastKid();
  int HasHiddenSeq = HasHiddenSeqSet.count(Type);
  bool ForceNewline = false;
  for (auto *Kid : *Nd) {
    if ((HasHiddenSeq && Kid == LastKid && isa<SequenceNode>(LastKid))
        || NeverSameLine.count(Kid->getType())) {
      fprintf(File, " ...");
      return;
    }
    ++Count;
    if (Count < KidsSameLine) {
      writeSpace();
      writeNodeAbbrev(Kid, false);
      continue;
    }
    if (Count == KidsSameLine) {
      writeSpace();
      ForceNewline = Count < NumKids;
      writeNode(Kid, false);
      if (Kid != LastKid)
        fprintf(File, " ...");
      return;
    }
    if (Count == 1 && EmbeddedInParent) {
      fprintf(File, " ...");
      return;
    }
    writeNode(Kid, false);
    return;
  }
}

void TextWriter::writeNodeAbbrev(const Node *Nd, bool AddNewline,
                                 bool EmbedInParent) {
  switch (NodeType Type = Nd->getType()) {
    default: {
      if (EmbedInParent) {
        fprintf(File, " ...");
      } else {
        Parenthesize _(this, Nd->getType(), AddNewline);
        writeNodeKidsAbbrev(Nd, false);
      }
      return;
    }
    case OpFile: {
      // Treat like hidden node. That is, visually just a list of s-expressions.
      fprintf(File, "(file ...)\n");
      return;
    }
    case OpInteger:
    case OpSymbol:
      writeNode(Nd, AddNewline, EmbedInParent);
      return;
  }
}

} // end of namespace filt

} // end of namespace wasm
