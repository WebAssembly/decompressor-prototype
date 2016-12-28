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
constexpr size_t MaxLine = 79; // allow for trailing space character.

void endLineIfOver(const size_t TabSize, FILE* Out, size_t &Indent) {
  if (Indent >= TabSize) {
    fputc('\n', Out);
    Indent = 0;
  }
}

void indentTo(const size_t TabSize, FILE* Out, size_t &Indent) {
  while (Indent < TabSize) {
    fputc(' ', Out);
    ++Indent;
  }
}

void printDescriptionContinue(const size_t TabSize,
                              FILE* Out,
                              size_t& Indent, const char* Description) {
  const char* Whitespace = " \t\n\0";
  while (Description && *Description != '\0') {
    while (Indent < MaxLine) {
      size_t Chunk = strcspn(Description, Whitespace);
      if (Chunk == 0) {
        if (*Description == '\0')
          return;
        if (Indent + 1 < MaxLine) {
          fputc(*(Description++), Out);
          ++Indent;
        } else {
          fputc('\n', Out);
          Indent = 0;
          ++Description;
        }
        continue;
      }
      if (!(Indent + Chunk < MaxLine || Indent == TabSize)) {
        fputc('\n', Out);
        Indent = 0;
        continue;
      }
      indentTo(TabSize, Out, Indent);
      for (size_t i = 0; i < Chunk; ++i) {
        fputc(*(Description++), Out);
        ++Indent;
      }
      if (*Description == '\0')
        break;
      if (strchr(Whitespace, *Description) != nullptr) {
        fputc(*(Description++), Out);
        ++Indent;
      }
    }
  }
  return;
}

void printDescription(const size_t TabSize,
                      FILE* Out,
                      size_t& Indent, const char* Description) {
  if (Description && *Description != '\0')
    endLineIfOver(TabSize, Out, Indent);
  printDescriptionContinue(TabSize, Out, Indent, Description);
}

} // end of anonymous namespace

namespace wasm {

namespace utils {

ArgsParser::Arg::~Arg() {}

void ArgsParser::Arg::describeDefault(FILE* Out, size_t TabSize,
                                      size_t& Indent)  const {
}

int ArgsParser::Arg::compare(const Arg& A) const {
  if (AddIndex < A.AddIndex)
    return -1;
  if (AddIndex > A.AddIndex)
    return 1;
  return 0;
}

ArgsParser::Optional::Optional(charstring Description)
    : Arg(ArgKind::Optional, Description),
      ShortName(0),
      LongName(nullptr) {}

ArgsParser::Optional::~Optional() {}

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

  if (ShortName != 0  && Opt->LongName != nullptr)
    return - ::compareNameCh(Opt->LongName, ShortName);

  return Arg::compare(A);
}

int ArgsParser::Optional::compareWithRequired(const Required& R) const {
  charstring ReqName = R.getOptionName();
  if (ReqName == nullptr)
    return  Arg::compare(R);

  if (LongName != nullptr)
    return compareNames(LongName, ReqName);

  if (ShortName != 0)
    return - ::compareNameCh(ReqName, ShortName);

  return Arg::compare(R);
}

void ArgsParser::Optional::describe(FILE* Out, size_t TabSize) const {
  size_t Indent = 0;
  indentTo(TabSize, Out, Indent);
  if (ShortName != 0) {
    fprintf(Out, "-%c", ShortName);
    Indent += 2;
  }
  if (LongName != nullptr) {
    if (ShortName != 0) {
      fputs(" | ", Out);
      Indent += 3;
    }
    fprintf(Out, "--%s", LongName);
    Indent += strlen(LongName) + 2;
  }
  if (OptionName != nullptr) {
    fprintf(stderr, " <%s>", OptionName);
    Indent += strlen(OptionName) + 1;
  }
  TabSize += TabWidth;
  printDescription(TabSize, Out, Indent,
                   Description == nullptr ? "" : Description);
  describeDefault(Out, TabSize, Indent);
  fputc('\n', Out);
}

ArgsParser::Bool::~Bool() {}

bool ArgsParser::Bool::select(charstring OptionValue) {
  // TODO(karlschimpf): All OptionValue to define value?
  Value = !DefaultValue;
  return false;
}

void ArgsParser::Bool::describeDefault(FILE* Out, size_t TabSize,
                                       size_t& Indent) const {
  printDescriptionContinue(TabSize, Out, Indent, " (default is ");
  printDescriptionContinue(TabSize, Out, Indent,
                           DefaultValue ? "true" : "false");
  printDescriptionContinue(TabSize, Out, Indent, ")");
}

ArgsParser::OptionalCharstring::~OptionalCharstring() {}

bool ArgsParser::OptionalCharstring::select(charstring OptionValue) {
  Value = OptionValue;
  return true;
}

void ArgsParser::OptionalCharstring::
describeDefault(FILE* Out, size_t TabSize, size_t& Indent) const {
  printDescriptionContinue(TabSize, Out, Indent, " (default is ");
  printDescriptionContinue(TabSize, Out, Indent,
                           strlen(DefaultValue) == 0 ? "''" : DefaultValue);
  printDescriptionContinue(TabSize, Out, Indent, ")");
}

int ArgsParser::Required::compare(const Arg& A) const {
  if (isa<Optional>(A))
    return - cast<Optional>(&A)->compareWithRequired(*this);

  return Arg::compare(A);
}

void ArgsParser::Required::describe(FILE* Out, size_t TabSize) const {
  size_t Indent = 0;
  indentTo(TabSize, Out, Indent);
  fprintf(Out, "<%s>", OptionName);
  Indent += strlen(OptionName) + 2;
  printDescription(TabSize + TabWidth, Out, Indent,
                   Description == nullptr ? "" : Description);
  fputc('\n', Out);
}

ArgsParser::RequiredCharstring::~RequiredCharstring() {}

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
    if (Opt->getShortName() != 0)
      OptionalsShort.push_back(Opt);
    if (Opt->getLongName() != nullptr)
      OptionalsLong.push_back(Opt);
  } else if (auto* Req = dyn_cast<Required>(&A)) {
    Requireds.push_back(Req);
  } else {
    fprintf(stderr, "Option not understood:\n");
    A.describe(stderr, TabWidth);
    Status = State::Bad;
  }
  return *this;
}

bool ArgsParser::parseShortName(const Optional* A,
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
  Leftover = Argument + 3;
  return true;
}

bool ArgsParser::parseLongName(const Optional* A,
                               charstring Argument,
                               charstring& Leftover) const {
  size_t ArgSize = strlen(Argument);
  if (ArgSize < 3)
    return false;
  if (Argument != strstr(Argument, "--"))
    return false;
  if (Argument + 2 != strstr(Argument, A->getLongName()))
    return false;
  size_t NameSize = strlen(A->getLongName());
  Leftover = (ArgSize == NameSize + 2) ? nullptr : Argument + NameSize + 1;
  return true;
}

ArgsParser::Optional* ArgsParser::parseNextShort(charstring Argument,
                                                 charstring& Leftover) {
  for (auto* A : OptionalsShort) {
    if (parseShortName(A, Argument, Leftover))
      return A;
  }
  return nullptr;
}

ArgsParser::Optional* ArgsParser::parseNextLong(charstring Argument,
                                                charstring& Leftover) {
  for (auto* A : OptionalsLong) {
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
  Optional* MatchingOption = parseNextShort(Argument, Leftover);
  if (MatchingOption == nullptr)
    MatchingOption = parseNextLong(Argument, Leftover);
  if (MatchingOption) {
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
  if (CurRequired < Requireds.size()) {
    if (!Requireds[CurRequired++]->select(Argument)) {
      fprintf(stderr, "Can't assign option:\n");
      Requireds[CurRequired - 1]->describe(stderr, TabWidth);
      showUsage();
      return;
    }
    return;
  }
  fprintf(stderr, "Argument '%s' not understood\n", Argument);
  showUsage();
  return;
}

ArgsParser::State ArgsParser::parse(int Argc, const char* Argv[]) {
  this->Argc = Argc;
  this->Argv = Argv;
  if (ExecName == nullptr && Argc)
    ExecName = Argv[0];
  CurArg = 1;
  CurRequired = 0;
  while (Status == State::Good && CurArg < Argc)
    parseNextArg();
  if (Status == State::Bad)
    showUsage();
  return Status;
}

void ArgsParser::showUsage() {
  Status = State::Usage;
  bool HasOptions = !(OptionalsShort.empty() && OptionalsLong.empty());
  fprintf(stderr, "\nUsage: %s", ExecName);
  if (HasOptions)
    fprintf(stderr, " [Options]");
  for (Required* A : Requireds)
    fprintf(stderr, " <%s>", A->getOptionName());
  fprintf(stderr, "\n");
  if (Description != nullptr) {
    fputc('\n', stderr);
    size_t Indent = 0;
    printDescription(TabWidth, stderr, Indent, Description);
    fputc('\n', stderr);
  }
  fputs("\nArguments:\n", stderr);
  std::sort(Args.begin(), Args.end(),
            [](Arg* Arg1, Arg* Arg2) {
              return Arg1->compare(*Arg2) < 0;
            });
  for (Arg* A : Args) {
    fputc('\n', stderr);
    A->describe(stderr, TabWidth);
  }
}

}  // end of namespace utils

}  // end of namespace wasm
