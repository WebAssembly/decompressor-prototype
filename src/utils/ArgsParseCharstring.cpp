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

// Implements command-line templates for charstring.

#include "utils/ArgsParse.h"

#include "utils/Defs.h"

namespace wasm {

namespace utils {

template <>
bool ArgsParser::Optional<charstring>::select(ArgsParser* Parser,
                                              charstring OptionValue) {
  if (!validOptionValue(Parser, OptionValue))
    return false;
  Value = OptionValue;
  return true;
}

template <>
void ArgsParser::Optional<charstring>::describeDefault(FILE* Out,
                                                       size_t TabSize,
                                                       size_t& Indent) const {
  if (DefaultValue == nullptr) {
    printDescriptionContinue(Out, TabSize, Indent, " (has no default value)");
    return;
  }
  printDescriptionContinue(Out, TabSize, Indent, " (default is '");
  printDescriptionContinue(Out, TabSize, Indent, DefaultValue);
  printDescriptionContinue(Out, TabSize, Indent, "')");
}

template <>
bool ArgsParser::Required<charstring>::select(ArgsParser* Parser,
                                              charstring OptionValue) {
  if (!validOptionValue(Parser, OptionValue))
    return false;
  Value = OptionValue;
  return true;
}

template <>
bool ArgsParser::OptionalVector<charstring>::select(ArgsParser* Parser,
                                                    charstring Add) {
  if (!validOptionValue(Parser, Add))
    return false;
  Values.push_back(Add);
  return true;
}

template <>
void ArgsParser::OptionalVector<charstring>::describeDefault(
    FILE* Out,
    size_t TabSize,
    size_t& Indent) const {}

template <>
bool ArgsParser::OptionalVectorSeparator<charstring>::select(ArgsParser* Parser,
                                                             charstring Add) {
  IndexVector.push_back(BaseVector.size());
  return false;
}

template <>
void ArgsParser::RequiredVector<charstring>::setOptionFound() {
  // Note: Later call to select() will add matched placement.
  if (Values.size() + 1 >= MinSize)
    RequiredArg::setOptionFound();
}

template <>
void ArgsParser::RequiredVector<charstring>::setPlacementFound(size_t&) {}

template <>
void ArgsParser::RequiredVector<charstring>::describeDefault(
    FILE* Out,
    size_t TabSize,
    size_t& Indent) const {
  switch (MinSize) {
    case 0:
      break;
    case 1:
      printDescriptionContinue(Out, TabSize, Indent,
                               " (must appear at least once)");
      break;
    default:
      printDescriptionContinue(Out, TabSize, Indent, " (must appear at least ");
      writeSize_t(Out, TabSize, Indent, MinSize);
      printDescriptionContinue(Out, TabSize, Indent, " times)");
      break;
  }
}

template <>
bool ArgsParser::RequiredVector<charstring>::select(ArgsParser* Parser,
                                                    charstring Add) {
  if (!validOptionValue(Parser, Add))
    return false;
  Values.push_back(Add);
  return true;
}

}  // end of namespace utils

}  // end of namespace wasm
