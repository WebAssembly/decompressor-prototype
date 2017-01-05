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

#include "intcomp/IntCountNode.h"
#include "sexp/Ast.h"

namespace wasm {

namespace intcomp {

class AbbreviationCodegen {
  AbbreviationCodegen() = delete;
  AbbreviationCodegen(const AbbreviationCodegen&) = delete;
  AbbreviationCodegen& operator=(const AbbreviationCodegen&) = delete;

 public:
  AbbreviationCodegen(CountNode::RootPtr Root,
                      interp::IntTypeFormat AbbrevFormat,
                      CountNode::PtrVector& Assignments);
  ~AbbreviationCodegen();

  std::shared_ptr<filt::SymbolTable> getCodeSymtab(bool ToRead);

 private:
  std::shared_ptr<filt::SymbolTable> Symtab;
  CountNode::RootPtr Root;
  interp::IntTypeFormat AbbrevFormat;
  CountNode::PtrVector& Assignments;
  bool ToRead;
  filt::Node* generateCasmFileHeader();
  filt::Node* generateWasmFileHeader();
  filt::Node* generateVoidFileHeader();
  void generateFile(filt::Node* SourceHeader,
                    filt::Node* TargetHeader);
  filt::Node* generateFileBody();
  filt::Node* generateFileFcn();
  filt::Node* generateAbbreviationRead();
  filt::Node* generateSwitchStatement();
  filt::Node* generateCase(size_t AbbrevIndex, CountNode::Ptr Nd);
  filt::Node* generateAction(CountNode::Ptr Nd);
  filt::Node* generateBlockAction(BlockCountNode* Blk);
  filt::Node* generateDefaultAction(DefaultCountNode* Default);
  filt::Node* generateDefaultMultipleAction();
  filt::Node* generateDefaultSingleAction();
  filt::Node* generateIntType(decode::IntType Value);
  filt::Node* generateIntLitAction(IntCountNode* Nd);
  filt::Node* generateIntLitActionRead(IntCountNode* Nd);
  filt::Node* generateIntLitActionWrite(IntCountNode* Nd);
  filt::Node* generateAbbrevFormat(interp::IntTypeFormat AbbrevFormat);
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVIATIONCODEGEN_H
