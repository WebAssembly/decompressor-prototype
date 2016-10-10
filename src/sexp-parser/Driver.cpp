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

using namespace wasm::filt;

namespace {
const char* ErrorLevelName[] = {"warning", "error", "fatal"};
}

const char* Driver::getName(ErrorLevel Level) {
  size_t Index = size_t(Level);
  if (Index < size(ErrorLevelName))
    return ErrorLevelName[Index];
  return "???";
}

bool Driver::parse(const std::string& Filename) {
  this->Filename = Filename;
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
