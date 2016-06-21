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

#include <cctype>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include <iostream>

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

char getHexCharForDigit(uint8_t Digit) {
  return Digit < 10 ? '0' + Digit : 'a' + (Digit - 10);
}

constexpr const char *IndentString = "  ";

} // end of anonyous namespace

bool TextWriter::UseNodeTypeNames = false;

TextWriter::TextWriter() {
  // Build fast lookup for number of arguments to write on same line.
  for (size_t i = 0; i < MaxNodeType; ++i) {
    KidCountSameLine.push_back(0);
  }
  for (size_t i = 0; i < NumNodeTypes; ++i) {
    AstTraitsType &Traits = AstTraits[i];
    KidCountSameLine[int(Traits.Type)] = Traits.NumTextArgs;
  }
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
    case OpFile: {
      // Treat like hidden node. That is, visually just a list of s-expressions.
      for (auto *Kid : *Node)
        writeNode(Kid, true);
      return;
    }
    case OpInteger: {
      Indent _(this, AddNewline);
      auto *Int = dynamic_cast<IntegerNode*>(Node);
      decode::IntType Value = Int->getValue();
      switch (Int->getFormat()) {
        case IntegerNode::SignedDecimal: {
          decode::SignedIntType SignedValue = decode::SignedIntType(Value);
          if (SignedValue < 0) {
            fputc('-', File);
            Value = decode::IntType(-SignedValue);
          }
        }
        // Intentionally fall to next case.
        case IntegerNode::Decimal: {
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
        case IntegerNode::Hexidecimal: {
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
      auto *Sym = dynamic_cast<SymbolNode*>(Node);
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

} // end of namespace filt

} // end of namespace wasm
