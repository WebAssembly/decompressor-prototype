// -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Defines an s-expression code generator for abbreviations.

#ifndef DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONCODEGEN_H
#define DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONCODEGEN_H

#include "intcomp/CountNode.h"
#include "sexp/Ast.h"

namespace wasm {

namespace intcomp {

class AbbreviationCodegen {
  AbbreviationCodegen() = delete;
  AbbreviationCodegen(const AbbreviationCodegen&) = delete;
  AbbreviationCodegen& operator=(const AbbreviationCodegen&) = delete;

 public:
  AbbreviationCodegen(const CompressionFlags& Flags,
                      CountNode::RootPtr Root,
                      utils::HuffmanEncoder::NodePtr EncodingRoot,
                      CountNode::PtrSet& Assignments,
                      bool ToRead);
  ~AbbreviationCodegen();

  std::shared_ptr<filt::SymbolTable> getCodeSymtab();

 private:
  const CompressionFlags& Flags;
  std::shared_ptr<filt::SymbolTable> Symtab;
  CountNode::RootPtr Root;
  utils::HuffmanEncoder::NodePtr EncodingRoot;
  CountNode::PtrSet& Assignments;
  bool ToRead;
  std::string CategorizeName;
  std::string OpcodeName;
  std::string ProcessName;
  std::string ValuesName;
  std::string OldName;

  filt::Node* generateHeader(filt::NodeType Type,
                             uint32_t MagicNumber,
                             uint32_t VersionNumber);
  filt::Node* generateStartFunction();
  filt::Node* generateAbbreviationRead();
  filt::Node* generateSwitchStatement();
  filt::Node* generateCase(size_t AbbrevIndex, CountNode::Ptr Nd);
  filt::Node* generateAction(CountNode::Ptr Nd);
  filt::Node* generateCallback(filt::PredefinedSymbol Sym);
  filt::Node* generateBlockAction(BlockCountNode* Blk);
  filt::Node* generateDefaultAction(DefaultCountNode* Default);
  filt::Node* generateDefaultMultipleAction();
  filt::Node* generateDefaultSingleAction();
  filt::Node* generateEnclosingAlg(charstring Name);
  filt::Node* generateIntType(decode::IntType Value);
  filt::Node* generateIntLitAction(IntCountNode* Nd);
  filt::Node* generateIntLitActionRead(IntCountNode* Nd);
  filt::Node* generateIntLitActionWrite(IntCountNode* Nd);
  filt::Node* generateAbbrevFormat(interp::IntTypeFormat AbbrevFormat);
  filt::Node* generateHuffmanEncoding(utils::HuffmanEncoder::NodePtr Root);
  void generateFunctions(filt::Algorithm* Alg);
  filt::Node* generateOpcodeFunction();
  filt::Node* generateCategorizeFunction();
  filt::Node* generateProcessFunction();
  filt::Node* generateValuesFunction();
  filt::Node* generateMapCase(decode::IntType Index, decode::IntType Value);
  filt::Node* generateRename(std::string Name);
  filt::Node* generateOld(std::string Name);
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONCODEGEN_H
