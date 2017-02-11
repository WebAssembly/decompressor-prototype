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

// Implements bool command-line arguments.

#include "utils/ArgsParse.h"

namespace wasm {

namespace utils {

template <>
bool ArgsParser::Optional<bool>::select(charstring OptionValue) {
  Value = !DefaultValue;
  return false;
}

template <>
void ArgsParser::Optional<bool>::describeDefault(FILE* Out,
                                                 size_t TabSize,
                                                 size_t& Indent) const {
  printDescriptionContinue(Out, TabSize, Indent, " (default is ");
  printDescriptionContinue(Out, TabSize, Indent,
                           DefaultValue ? "true" : "false");
  printDescriptionContinue(Out, TabSize, Indent, ")");
}

bool ArgsParser::Toggle::select(charstring OptionValue) {
  Value = !Value;
  return false;
}

void ArgsParser::Toggle::describeDefault(FILE* Out,
                                         size_t TabSize,
                                         size_t& Indent) const {
  Optional<bool>::describeDefault(Out, TabSize, Indent);
  printDescriptionContinue(Out, TabSize, Indent,
                           " (each occurrence toggles value)");
}

}  // end of namespace utils

}  // end of namespace wasm
