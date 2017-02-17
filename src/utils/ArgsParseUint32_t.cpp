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

// Implements command-line templates for unint32_t

#include "utils/ArgsParse.h"

#include <cctype>
#include <cstdlib>

namespace wasm {

namespace utils {

template <>
bool ArgsParser::Optional<uint32_t>::select(ArgsParser* Parser,
                                            charstring OptionValue) {
  if (!validOptionValue(Parser, OptionValue))
    return false;
  Value = uint32_t(atoll(OptionValue));
  return true;
}

template <>
void ArgsParser::Optional<uint32_t>::describeDefault(FILE* Out,
                                                     size_t TabSize,
                                                     size_t& Indent) const {
  printDescriptionContinue(Out, TabSize, Indent, " (default is ");
  writeSize_t(Out, TabSize, Indent, DefaultValue);
  printDescriptionContinue(Out, TabSize, Indent, ")");
}

}  // end of namespace utils

}  // end of namespace wasm
