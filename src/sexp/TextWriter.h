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

/* Defines a textual writer of filter s-expressions */

#ifndef DECOMPRESSOR_SRC_EXP_TEXTWRITER_H
#define DECOMPRESSOR_SRC_EXP_TEXTWRITER_H

#include "sexp/Ast.h"
#include "utils/Defs.h"

#include <cstdio>
#include <unordered_set>

namespace wasm {

namespace filt {

class TextWriter {
  TextWriter(const TextWriter&) = delete;
  TextWriter& operator=(const TextWriter&) = delete;

  class Indent {
   public:
    // Indent if new line. Add newline, if requested, when going out of scope.
    Indent(TextWriter* Writer, bool AddNewline);
    ~Indent();

   private:
    TextWriter* Writer;
    bool AddNewline;
  };

  class Parenthesize {
   public:
    // Indent if new line. Indent elements between constructor and destructor by
    // one. Add newline, if requested, when going out of scopre.
    Parenthesize(TextWriter* Writer, NodeType Type, bool AddNewline);
    ~Parenthesize();

   private:
    TextWriter* Writer;
    bool AddNewline;
  };

 public:
  // When true use getNodeTypeName() instead of getNodeSexpName() for node
  // names.
  static bool UseNodeTypeNames;
  TextWriter();

  // Pretty prints s-expression (defined by Root) to File.
  void write(FILE* File, const Node* Root);

  // Prints one-line summary of s-expression (defined by Root) to File.
  void writeAbbrev(FILE* File, const Node* Root);

 private:
  FILE* File;
  size_t IndentCount;
  bool LineEmpty;
  std::vector<int> KidCountSameLine;
  std::vector<int> MaxKidCountSameLine;
  std::unordered_set<int> HasHiddenSeqSet;
  std::unordered_set<int> NeverSameLine;

  void initialize(FILE* File);

  void writeNode(const Node* Node, bool AddNewline, bool EmbedInParent = false);
  void writeNodeKids(const Node* Node, bool EmbeddedInParent);

  void writeNodeAbbrev(const Node* Node,
                       bool AddNewline,
                       bool EmbedInParent = false);
  void writeNodeKidsAbbrev(const Node* Node, bool EmbeddedInParent);

  void writeIndent();

  void writeNewline() {
    fputc('\n', File);
    LineEmpty = true;
  }

  void maybeWriteNewline(bool Yes) {
    if (Yes)
      writeNewline();
  }

  void writeSpace() {
    fputc(' ', File);
    LineEmpty = false;
  }
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_EXP_TEXTWRITER_H
