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

// Implements a command-line argument parser.

#include "utils/ArgsParse.h"

#include "utils/Defs.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>

namespace {

int compareNames(const char* Name1, const char* Name2) {
  while (*Name1 && *Name2) {
    int Diff = tolower(*Name1) - tolower(*Name2);
    if (Diff != 0)
      return Diff;
    Diff = *(Name1++) - *(Name2++);
    if (Diff != 0)
      return Diff;
  }
  if (*Name1)
    return 1;
  if (*Name2)
    return -1;
  return 0;
}

int compareNameCh(const char* Name, char Ch) {
  char Buffer[2];
  Buffer[0] = Ch;
  Buffer[1] = '\0';
  return compareNames(Name, Buffer);
}

constexpr size_t TabWidth = 8;
constexpr size_t MaxLine = 79;  // allow for trailing space character.

void endLineIfOver(const size_t TabSize, FILE* Out, size_t& Indent) {
  if (Indent >= TabSize) {
    fputc('\n', Out);
    Indent = 0;
  }
}

void indentTo(const size_t TabSize, FILE* Out, size_t& Indent) {
  while (Indent < TabSize) {
    fputc(' ', Out);
    ++Indent;
  }
}

void writeNewline(FILE* Out, size_t& Indent) {
  fputc('\n', Out);
  Indent = 0;
}

void writeChar(const size_t TabSize, FILE* Out, size_t& Indent, char Ch) {
  if (Indent >= MaxLine) {
    writeNewline(Out, Indent);
  }
  switch (Ch) {
    case '\t': {
      indentTo(TabSize, Out, Indent);
      size_t Spaces = TabWidth - (Indent % TabWidth);
      for (size_t i = 0; i < Spaces; ++i)
        writeChar(TabSize, Out, Indent, ' ');
      break;
    }
    case '\n':
      writeNewline(Out, Indent);
      break;
    case ' ':
      if (Indent > TabSize) {
        fputc(' ', Out);
        ++Indent;
      }
      break;
    default:
      fputc(Ch, Out);
      ++Indent;
      break;
  }
}

void writeChunk(const size_t TabSize,
                FILE* Out,
                size_t& Indent,
                const char* string,
                size_t Chunk) {
  for (size_t i = 0; i < Chunk; ++i)
    writeChar(TabSize, Out, Indent, string[i]);
}

void writeCharstring(const size_t TabSize,
                     FILE* Out,
                     size_t& Indent,
                     const char* string) {
  writeChunk(TabSize, Out, Indent, string, strlen(string));
}

void printDescriptionContinue(const size_t TabSize,
                              FILE* Out,
                              size_t& Indent,
                              const char* Description) {
  const char* Whitespace = " \t\n";
  while (Description && *Description != '\0') {
    bool LoopApplied = false;
    while (Indent < MaxLine) {
      LoopApplied = true;
      size_t Chunk = strcspn(Description, Whitespace);
      if (Chunk == 0) {
        if (*Description == '\0')
          return;
        writeChar(TabSize, Out, Indent, *(Description++));
        continue;
      }
      if (!(Indent + Chunk < MaxLine || Indent == TabSize)) {
        writeNewline(Out, Indent);
        continue;
      }
      indentTo(TabSize, Out, Indent);
      writeChunk(TabSize, Out, Indent, Description, Chunk);
      Description += Chunk;
    }
    if (!LoopApplied)
      writeNewline(Out, Indent);
  }
  return;
}

void printDescription(const size_t TabSize,
                      FILE* Out,
                      size_t& Indent,
                      const char* Description) {
  if (Description && *Description != '\0')
    endLineIfOver(TabSize, Out, Indent);
  printDescriptionContinue(TabSize, Out, Indent, Description);
}

}  // end of anonymous namespace

namespace wasm {

namespace utils {

ArgsParser::Arg::~Arg() {
}

void ArgsParser::Arg::describe(FILE* Out, size_t TabSize) const {
  size_t Indent = 0;
  indentTo(TabSize, Out, Indent);
  if (ShortName != 0) {
    writeChar(TabSize, Out, Indent, '-');
    writeChar(TabSize, Out, Indent, ShortName);
  }
  if (LongName != nullptr) {
    if (Indent > TabSize) {
      writeCharstring(TabSize, Out, Indent, " |");
    }
    writeCharstring(TabSize, Out, Indent, " --");
    writeCharstring(TabSize, Out, Indent, LongName);
  }
  if (OptionName != nullptr) {
    writeChar(TabSize, Out, Indent, ' ');
    writeCharstring(TabSize, Out, Indent, OptionName);
  }
  TabSize += TabWidth;
  printDescription(TabSize, Out, Indent,
                   Description == nullptr ? "" : Description);
  describeDefault(Out, TabSize, Indent);
  if (isa<Required>(this))
    printDescriptionContinue(TabSize, Out, Indent, " (required)");
  writeNewline(Out, Indent);
}

void ArgsParser::Arg::describeDefault(FILE* Out,
                                      size_t TabSize,
                                      size_t& Indent) const {
}

int ArgsParser::Arg::compare(const Arg& A) const {
  if (AddIndex < A.AddIndex)
    return -1;
  if (AddIndex > A.AddIndex)
    return 1;
  return 0;
}

ArgsParser::Optional::Optional(charstring Description)
    : Arg(ArgKind::Optional, Description) {
}

ArgsParser::Optional::~Optional() {
}

int ArgsParser::Optional::compare(const Arg& A) const {
  if (isa<Required>(&A))
    return compareWithRequired(*cast<Required>(&A));

  const Optional* Opt = cast<Optional>(&A);
  if (LongName != nullptr && Opt->LongName != nullptr)
    return compareNames(LongName, Opt->LongName);

  if (ShortName != 0 && Opt->ShortName != 0)
    return ShortName - Opt->ShortName;

  if (LongName != nullptr && Opt->ShortName != 0)
    return ::compareNameCh(LongName, Opt->ShortName);

  if (ShortName != 0 && Opt->LongName != nullptr)
    return -::compareNameCh(Opt->LongName, ShortName);

  return Arg::compare(A);
}

int ArgsParser::Optional::compareWithRequired(const Required& R) const {
  charstring ReqName = R.getOptionName();
  if (ReqName == nullptr)
    return Arg::compare(R);

  if (LongName != nullptr)
    return compareNames(LongName, ReqName);

  if (ShortName != 0)
    return -::compareNameCh(ReqName, ShortName);

  return Arg::compare(R);
}

ArgsParser::Bool::~Bool() {
}

bool ArgsParser::Bool::select(charstring OptionValue) {
  // TODO(karlschimpf): All OptionValue to define value?
  if (Toggle)
    Value = !Value;
  else
    Value = !DefaultValue;
  return false;
}

void ArgsParser::Bool::describeDefault(FILE* Out,
                                       size_t TabSize,
                                       size_t& Indent) const {
  printDescriptionContinue(TabSize, Out, Indent, " (default is ");
  printDescriptionContinue(TabSize, Out, Indent,
                           DefaultValue ? "true" : "false");
  printDescriptionContinue(TabSize, Out, Indent, ")");
  if (Toggle)
    printDescriptionContinue(TabSize, Out, Indent,
                             " (each occurrence toggles vaue)");
}

ArgsParser::OptionalCharstring::~OptionalCharstring() {
}

bool ArgsParser::OptionalCharstring::select(charstring OptionValue) {
  Value = OptionValue;
  return true;
}

void ArgsParser::OptionalCharstring::describeDefault(FILE* Out,
                                                     size_t TabSize,
                                                     size_t& Indent) const {
  if (DefaultValue == nullptr) {
    printDescriptionContinue(TabSize, Out, Indent, " (has no default value)");
    return;
  }
  printDescriptionContinue(TabSize, Out, Indent, " (default is '");
  printDescriptionContinue(TabSize, Out, Indent, DefaultValue);
  printDescriptionContinue(TabSize, Out, Indent, "')");
}

int ArgsParser::Required::compare(const Arg& A) const {
  if (isa<Optional>(A))
    return -cast<Optional>(&A)->compareWithRequired(*this);

  return Arg::compare(A);
}

ArgsParser::RequiredCharstring::~RequiredCharstring() {
}

bool ArgsParser::RequiredCharstring::select(charstring OptionValue) {
  Value = OptionValue;
  return true;
}

ArgsParser::ArgsParser(charstring Description)
    : ExecName(nullptr),
      Description(Description),
      Help(false),
      HelpFlag(Help, "Describe how to use"),
      CurArg(0),
      Status(State::Good) {
  HelpFlag.setDefault(false).setShortName('h').setLongName("help");
  add(HelpFlag);
}

ArgsParser& ArgsParser::add(Arg& A) {
  A.setAddIndex(Args.size());
  Args.push_back(&A);
  if (auto* Opt = dyn_cast<Optional>(&A)) {
    if (Opt->getShortName() == 0 && Opt->getLongName() == nullptr) {
      fprintf(stderr, "Can't add option without Name:\n");
      A.describe(stderr, TabWidth);
      Status = State::Bad;
    }
  }
  bool IsPlacement = true;  // until proven otherwise.
  if (A.getShortName() != 0) {
    Shorts.push_back(&A);
    IsPlacement = false;
  }
  if (A.getLongName() != nullptr) {
    Longs.push_back(&A);
    IsPlacement = false;
  }
  if (IsPlacement) {
    if (A.getOptionName() != nullptr) {
      Placements.push_back(&A);
    } else {
      fprintf(stderr, "Can't categorize option:\n");
      A.describe(stderr, TabWidth);
      Status = State::Bad;
    }
  }
  if (auto* R = dyn_cast<Required>(&A))
    Requireds.push_back(R);
  return *this;
}

bool ArgsParser::parseShortName(const Arg* A,
                                charstring Argument,
                                charstring& Leftover) const {
  size_t ArgSize = strlen(Argument);
  if (ArgSize < 2)
    return false;
  if (Argument[0] != '-')
    return false;
  if (Argument[1] == '-')
    return false;
  if (Argument[1] != A->getShortName())
    return false;
  if (ArgSize == 2) {
    Leftover = nullptr;
    return true;
  }
  if (Argument[2] != '=')
    return false;
  if (A->getOptionName() == nullptr)
    return false;
  Leftover = Argument + 3;
  return true;
}

bool ArgsParser::parseLongName(const Arg* A,
                               charstring Argument,
                               charstring& Leftover) const {
  size_t ArgSize = strlen(Argument);
  if (ArgSize < 3)
    return false;
  if (Argument != strstr(Argument, "--"))
    return false;
  size_t NameSize = strlen(A->getLongName()) + 2;
  if (Argument + 2 != strstr(Argument, A->getLongName()))
    return false;
  if (ArgSize == NameSize) {
    Leftover = nullptr;
    return true;
  }
  if (Argument[NameSize] != '=')
    return false;
  if (A->getOptionName() == nullptr)
    return false;
  Leftover = Argument + NameSize + 1;
  return true;
}

ArgsParser::Arg* ArgsParser::parseNextShort(charstring Argument,
                                            charstring& Leftover) {
  for (auto* A : Shorts) {
    if (parseShortName(A, Argument, Leftover))
      return A;
  }
  return nullptr;
}

ArgsParser::Arg* ArgsParser::parseNextLong(charstring Argument,
                                           charstring& Leftover) {
  for (auto* A : Longs) {
    if (parseLongName(A, Argument, Leftover))
      return A;
  }
  return nullptr;
}

void ArgsParser::parseNextArg() {
  if (CurArg == Argc)
    return;
  if (Status == State::Usage)
    return;
  const char* Argument = Argv[CurArg++];
  if (Argument == nullptr || strlen(Argument) == 0)
    return;
  charstring Leftover = nullptr;
  Arg* MatchingOption = parseNextShort(Argument, Leftover);
  if (MatchingOption == nullptr)
    MatchingOption = parseNextLong(Argument, Leftover);
  if (MatchingOption) {
    MatchingOption->setOptionFound(true);
    bool MaybeUseNextArg = false;
    if (Leftover == nullptr && CurArg < Argc) {
      MaybeUseNextArg = true;
      Leftover = Argv[CurArg];
    }
    if (MatchingOption->select(Leftover) && MaybeUseNextArg)
      ++CurArg;
    if (MatchingOption == &HelpFlag)
      showUsage();
    return;
  }
  if (CurPlacement < Placements.size()) {
    Arg* Placement = Placements[CurPlacement++];
    Placement->setOptionFound(true);
    if (!Placement->select(Argument)) {
      fprintf(stderr, "Can't assign option:\n");
      Placement->describe(stderr, TabWidth);
      Status = State::Bad;
      return;
    }
    return;
  }
  fprintf(stderr, "Argument '%s' not understood\n", Argument);
  Status = State::Bad;
  return;
}

ArgsParser::State ArgsParser::parse(int Argc, const char* Argv[]) {
  this->Argc = Argc;
  this->Argv = Argv;
  if (ExecName == nullptr && Argc)
    ExecName = Argv[0];
  CurArg = 1;
  CurPlacement = 0;
  while (Status == State::Good && CurArg < Argc)
    parseNextArg();
  if (Status == State::Good) {
    for (Arg* A : Requireds)
      if (!A->getOptionFound()) {
        fprintf(stderr, "Required option not found:\n");
        A->describe(stderr, TabWidth);
        Status = State::Bad;
      }
  }
  return Status;
}

void ArgsParser::showUsage() {
  Status = State::Usage;
  bool HasOptions = !(Shorts.empty() && Longs.empty());
  size_t Indent = 0;
  printDescriptionContinue(0, stderr, Indent, "Usage:");
  writeNewline(stderr, Indent);
  writeNewline(stderr, Indent);
  printDescription(TabWidth, stderr, Indent, ExecName);
  if (HasOptions)
    printDescriptionContinue(TabWidth, stderr, Indent, " [Options]");
  for (Arg* A : Placements) {
    printDescriptionContinue(TabWidth, stderr, Indent, " ");
    printDescriptionContinue(TabWidth, stderr, Indent, A->getOptionName());
  }
  writeNewline(stderr, Indent);
  if (Description != nullptr) {
    writeNewline(stderr, Indent);
    printDescription(TabWidth, stderr, Indent, Description);
    writeNewline(stderr, Indent);
  }
  if (Args.empty())
    return;
  writeNewline(stderr, Indent);
  printDescription(0, stderr, Indent, "Arguments:");
  writeNewline(stderr, Indent);
  std::sort(Args.begin(), Args.end(),
            [](Arg* Arg1, Arg* Arg2) { return Arg1->compare(*Arg2) < 0; });
  for (Arg* A : Args) {
    writeNewline(stderr, Indent);
    A->describe(stderr, TabWidth);
  }
  writeNewline(stderr, Indent);
}

}  // end of namespace utils

}  // end of namespace wasm
