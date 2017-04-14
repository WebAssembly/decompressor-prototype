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

#include "sexp-parser/Driver.h"

namespace wasm {

namespace filt {

namespace {
const char* ErrorLevelName[] = {"warning", "error", "fatal"};
}  // end of anonymous namespace

void Driver::appendArgument(Node* Nd, Node* Arg) {
  Node* LastKid = Nd->getLastKid();
  auto* Seq = dyn_cast<Sequence>(LastKid);
  if (Seq == nullptr) {
    Seq = create<Sequence>();
    Seq->append(LastKid);
    Nd->setLastKid(Seq);
  }
  Seq->append(Arg);
}

const char* Driver::getName(ErrorLevel Level) {
  size_t Index = size_t(Level);
  if (Index < size(ErrorLevelName))
    return ErrorLevelName[Index];
  return "???";
}

bool Driver::parse(const std::string& Filename) {
  SymbolTable::SharedPtr FirstSymtab = Table;
  Enclosing.erase();
  size_t LastSlash = Filename.find_last_of("/\\");
  if (LastSlash == std::string::npos)
    BaseFilename.erase();
  else
    BaseFilename = Filename.substr(0, LastSlash + 1);
  bool Success = true;
  std::string NextFile = Filename;
  SymbolTable::SharedPtr EnclosedSymtab;
  if (TraceFilesParsed)
    fprintf(stderr, "Parsing algiorithm: '%s'\n", Filename.c_str());
  std::set<std::string> ParsedFilenames;
  while (true) {
    if (ParsedFilenames.count(NextFile) > 0) {
      fprintf(stderr, "Algorithm encloses self: %s\n", NextFile.c_str());
      Success = false;
      break;
    }
    ParsedFilenames.insert(NextFile);
    Success = parseOneFile(NextFile);
    if (!Success)
      break;
    if (Enclosing.empty())
      break;
    EnclosedSymtab = Table;
    Table = std::make_shared<SymbolTable>();
    EnclosedSymtab->setEnclosingScope(Table);
    NextFile = BaseFilename + Enclosing;
    Enclosing.erase();
    if (TraceFilesParsed)
      fprintf(stderr, "Parsing enclosing algorithm: '%s'\n", NextFile.c_str());
  }
  Table = FirstSymtab;
  ParsedAst = Table->getAlgorithm();
  return Success;
}

bool Driver::parseOneFile(std::string& Filename) {
  this->Filename = Filename;
  Loc.initialize(&Filename);
  ParsedAst = nullptr;
  Begin();
  wasm::filt::Parser parser(*this);
  parser.set_debug_level(TraceParsing);
  int Result = parser.parse();
  End();
  return Result == 0 && !ErrorsReported;
}

void Driver::report(ErrorLevel Level,
                    const wasm::filt::location& L,
                    const std::string& M) {
  std::cerr << getName(Level) << ": " << L << ": " << M << std::endl;
  switch (Level) {
    case ErrorLevel::Warn:
      return;
    case ErrorLevel::Error:
      ErrorsReported = true;
      return;
    case ErrorLevel::Fatal:
      return;
  }
}

void Driver::tokenError(const std::string& Token) {
  std::string Message("Invalid token'");
  error(std::string("invalidToken '") + Token + "'");
}

}  // end of namespace filt

}  // end of namespace wasm
