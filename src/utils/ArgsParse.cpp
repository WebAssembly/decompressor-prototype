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
#include <cstdlib>
#include <cstring>

namespace wasm {

namespace {

int compareNames(charstring Name1, charstring Name2) {
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

int compareNameCh(charstring Name, char Ch) {
  char Buffer[2];
  Buffer[0] = Ch;
  Buffer[1] = '\0';
  return compareNames(Name, Buffer);
}

charstring StateName[]{"Bad", "Usage", "Good"};

charstring ArgKindName[]{"Optional", "Required"};

}  // end of anonymous namespace

namespace utils {

const size_t ArgsParser::TabWidth = 8;
const size_t ArgsParser::MaxLine = 79;  // allow for trailing space character.

charstring ArgsParser::getName(State S) {
  return StateName[size_t(S)];
}

charstring ArgsParser::getName(ArgKind K) {
  return ArgKindName[size_t(K)];
}

FILE* ArgsParser::error() {
  Status = State::Bad;
  return stderr;
}

void ArgsParser::endLineIfOver(FILE* Out,
                               const size_t TabSize,
                               size_t& Indent) {
  if (Indent >= TabSize) {
    fputc('\n', Out);
    Indent = 0;
  }
}

void ArgsParser::indentTo(FILE* Out, const size_t TabSize, size_t& Indent) {
  while (Indent < TabSize) {
    fputc(' ', Out);
    ++Indent;
  }
}

void ArgsParser::writeNewline(FILE* Out, size_t& Indent) {
  fputc('\n', Out);
  Indent = 0;
}

void ArgsParser::writeChar(FILE* Out,
                           const size_t TabSize,
                           size_t& Indent,
                           char Ch) {
  if (Indent >= MaxLine) {
    writeNewline(Out, Indent);
  }
  switch (Ch) {
    case '\t': {
      indentTo(Out, TabSize, Indent);
      size_t Spaces = TabWidth - (Indent % TabWidth);
      for (size_t i = 0; i < Spaces; ++i)
        writeChar(Out, TabSize, Indent, ' ');
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

void ArgsParser::writeChunk(FILE* Out,
                            const size_t TabSize,
                            size_t& Indent,
                            charstring String,
                            size_t Chunk) {
  for (size_t i = 0; i < Chunk; ++i)
    writeChar(Out, TabSize, Indent, String[i]);
}

void ArgsParser::writeCharstring(FILE* Out,
                                 const size_t TabSize,
                                 size_t& Indent,
                                 charstring String) {
  writeChunk(Out, TabSize, Indent, String, strlen(String));
}

void ArgsParser::writeSize_t(FILE* Out,
                             const size_t TabSize,
                             size_t& Indent,
                             size_t Value) {
  if (Indent + sizeof(size_t) >= MaxLine) {
    writeNewline(Out, Indent);
    indentTo(Out, TabSize, Indent);
  }
  fprintf(Out, "%" PRIuMAX "", uintmax_t(Value));
}

void ArgsParser::printDescriptionContinue(FILE* Out,
                                          const size_t TabSize,
                                          size_t& Indent,
                                          charstring Description) {
  charstring Whitespace = " \t\n";
  while (Description && *Description != '\0') {
    bool LoopApplied = false;
    while (Indent < MaxLine) {
      LoopApplied = true;
      size_t Chunk = strcspn(Description, Whitespace);
      if (Chunk == 0) {
        if (*Description == '\0')
          return;
        writeChar(Out, TabSize, Indent, *(Description++));
        continue;
      }
      if (!(Indent + Chunk < MaxLine || Indent == TabSize)) {
        writeNewline(Out, Indent);
        continue;
      }
      indentTo(Out, TabSize, Indent);
      writeChunk(Out, TabSize, Indent, Description, Chunk);
      Description += Chunk;
    }
    if (!LoopApplied)
      writeNewline(Out, Indent);
  }
  return;
}

void ArgsParser::printDescription(FILE* Out,
                                  const size_t TabSize,
                                  size_t& Indent,
                                  charstring Description) {
  if (Description && *Description != '\0')
    endLineIfOver(Out, TabSize, Indent);
  printDescriptionContinue(Out, TabSize, Indent, Description);
}

void ArgsParser::Arg::setOptionFound() {
  OptionFound = true;
}

void ArgsParser::Arg::setPlacementFound(size_t& CurPlacement) {
  ++CurPlacement;
}

bool ArgsParser::Arg::validOptionValue(ArgsParser* Parser,
                                       charstring OptionValue) {
  if (OptionValue != nullptr)
    return true;
  FILE* Out = Parser->error();
  fprintf(Out, "Malformed specification: No option value specified!\n");
  describe(Out, TabWidth);
  return false;
}

void ArgsParser::Arg::describe(FILE* Out, size_t TabSize) const {
  size_t Indent = 0;
  indentTo(Out, TabSize, Indent);
  bool HasName = false;
  if (ShortName != 0) {
    writeChar(Out, TabSize, Indent, '-');
    writeChar(Out, TabSize, Indent, ShortName);
    describeOptionName(Out, TabSize, Indent);
    HasName = true;
  }
  if (LongName != nullptr) {
    if (HasName)
      writeCharstring(Out, TabSize, Indent, " |");
    writeCharstring(Out, TabSize, Indent, " --");
    writeCharstring(Out, TabSize, Indent, LongName);
    describeOptionName(Out, TabSize, Indent);
    HasName = true;
  }
  if (!HasName) {
    describeOptionName(Out, TabSize, Indent);
  }
  if (IsRepeatable)
    writeCharstring(Out, TabSize, Indent, " ...");
  TabSize += TabWidth;
  printDescription(Out, TabSize, Indent,
                   Description == nullptr ? "" : Description);
  describeDefault(Out, TabSize, Indent);
  if (isa<RequiredArg>(this))
    printDescriptionContinue(Out, TabSize, Indent, " (required)");
  writeChar(Out, TabSize, Indent, '.');
  writeNewline(Out, Indent);
}

void ArgsParser::Arg::describeDefault(FILE* Out,
                                      size_t TabSize,
                                      size_t& Indent) const {
}

void ArgsParser::Arg::describeOptionName(FILE* Out,
                                         size_t TabSize,
                                         size_t& Indent) const {
  if (OptionName != nullptr) {
    writeChar(Out, TabSize, Indent, ' ');
    writeCharstring(Out, TabSize, Indent, OptionName);
  }
}

int ArgsParser::Arg::compare(const Arg& A) const {
  if (AddIndex < A.AddIndex)
    return -1;
  if (AddIndex > A.AddIndex)
    return 1;
  return 0;
}

ArgsParser::OptionalArg::OptionalArg() : Arg(ArgKind::Optional) {
}

int ArgsParser::OptionalArg::compare(const Arg& A) const {
  if (isa<RequiredArg>(&A))
    return compareWithRequired(*cast<RequiredArg>(&A));

  const OptionalArg* Opt = cast<OptionalArg>(&A);
  if (LongName != nullptr && Opt->LongName != nullptr)
    return compareNames(LongName, Opt->LongName);

  if (ShortName != 0 && Opt->ShortName != 0)
    return ShortName - Opt->ShortName;

  if (LongName != nullptr && Opt->ShortName != 0)
    return compareNameCh(LongName, Opt->ShortName);

  if (ShortName != 0 && Opt->LongName != nullptr)
    return -compareNameCh(Opt->LongName, ShortName);

  return Arg::compare(A);
}

int ArgsParser::OptionalArg::compareWithRequired(const RequiredArg& R) const {
  charstring ReqName = R.getOptionName();
  if (ReqName == nullptr)
    return Arg::compare(R);

  if (LongName != nullptr)
    return compareNames(LongName, ReqName);

  if (ShortName != 0)
    return -compareNameCh(ReqName, ShortName);

  return Arg::compare(R);
}

int ArgsParser::RequiredArg::compare(const Arg& A) const {
  if (isa<OptionalArg>(A))
    return -cast<OptionalArg>(&A)->compareWithRequired(*this);

  return Arg::compare(A);
}

ArgsParser::ArgsParser(charstring Description)
    : ExecName(nullptr),
      Description(Description),
      Help(false),
      HelpFlag(Help),
      CurArg(0),
      Status(State::Good),
      TraceProgress(false) {
  HelpFlag.setDefault(false)
      .setShortName('h')
      .setLongName("help")
      .setDescription("Describe how to use");
  add(HelpFlag);
}

ArgsParser& ArgsParser::add(Arg& A) {
  A.setAddIndex(Args.size());
  if (TraceProgress) {
    fprintf(stderr, "Add:\n");
    A.describe(stderr, TabWidth);
  }
  Args.push_back(&A);
  if (auto* Opt = dyn_cast<OptionalArg>(&A)) {
    if (Opt->getShortName() == 0 && Opt->getLongName() == nullptr) {
      FILE* Out = error();
      fprintf(Out, "Can't add option without Name:\n");
      A.describe(Out, TabWidth);
      Status = State::Bad;
    }
  }
  if (auto* R = dyn_cast<RequiredArg>(&A))
    RequiredArgs.push_back(R);

  bool IsPlacement = true;  // until proven otherwise.
  if (A.getShortName() != 0) {
    ShortArgs.push_back(&A);
    IsPlacement = false;
  }
  if (A.getLongName() != nullptr) {
    LongArgs.push_back(&A);
    IsPlacement = false;
  }
  if (IsPlacement) {
    if (A.getOptionName() != nullptr) {
      PlacementArgs.push_back(&A);
    } else {
      FILE* Out = error();
      fprintf(Out, "Can't categorize option:\n");
      A.describe(Out, TabWidth);
      Status = State::Bad;
    }
  }
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
  for (auto* A : ShortArgs) {
    if (parseShortName(A, Argument, Leftover))
      return A;
  }
  return nullptr;
}

ArgsParser::Arg* ArgsParser::parseNextLong(charstring Argument,
                                           charstring& Leftover) {
  for (auto* A : LongArgs) {
    if (parseLongName(A, Argument, Leftover))
      return A;
  }
  return nullptr;
}

void ArgsParser::parseNextArg() {
  if (CurArg == Argc)
    return;
  if (TraceProgress)
    fprintf(stderr, "parse arg[%d] = '%s'\n", CurArg, Argv[CurArg]);
  if (Status == State::Usage)
    return;
  charstring Argument = Argv[CurArg++];
  if (Argument == nullptr || strlen(Argument) == 0)
    return;
  charstring Leftover = nullptr;
  Arg* MatchingOption = parseNextShort(Argument, Leftover);
  if (MatchingOption == nullptr)
    MatchingOption = parseNextLong(Argument, Leftover);
  if (MatchingOption) {
    MatchingOption->setOptionFound();
    bool MaybeUseNextArg = false;
    if (Leftover == nullptr && CurArg < Argc) {
      MaybeUseNextArg = true;
      Leftover = Argv[CurArg];
    }
    if (MatchingOption->select(this, Leftover) && MaybeUseNextArg)
      ++CurArg;
    if (TraceProgress) {
      fprintf(stderr, "Matched:\n");
      MatchingOption->describe(stderr, TabWidth);
    }
    if (MatchingOption == &HelpFlag)
      showUsage();
    return;
  }
  if (CurPlacement < PlacementArgs.size()) {
    Arg* Placement = PlacementArgs[CurPlacement];
    Placement->setPlacementFound(CurPlacement);
    Placement->setOptionFound();
    if (TraceProgress) {
      fprintf(stderr, "Matched:\n");
      Placement->describe(stderr, TabWidth);
    }
    if (!Placement->select(this, Argument)) {
      FILE* Out = error();
      fprintf(Out, "Can't assign option:\n");
      Placement->describe(Out, TabWidth);
      Status = State::Bad;
      return;
    }
    return;
  }
  fprintf(error(), "Argument '%s' not understood\n", Argument);
  Status = State::Bad;
  return;
}

ArgsParser::State ArgsParser::parse(int Argc, charstring Argv[]) {
  this->Argc = Argc;
  this->Argv = Argv;
  if (ExecName == nullptr && Argc)
    ExecName = Argv[0];
  CurArg = 1;
  CurPlacement = 0;
  while (Status == State::Good && CurArg < Argc)
    parseNextArg();
  if (Status == State::Good) {
    for (Arg* A : RequiredArgs)
      if (!A->getOptionFound()) {
        FILE* Out = error();
        fprintf(Out, "Required option not found:\n");
        A->describe(Out, TabWidth);
        Status = State::Bad;
      }
  }
  if (TraceProgress)
    fprintf(stderr, "Status = %s\n", getName(Status));
  return Status;
}

void ArgsParser::showUsage() {
  Status = State::Usage;
  bool HasOptions = !(ShortArgs.empty() && LongArgs.empty());
  size_t Indent = 0;
  printDescriptionContinue(stderr, 0, Indent, "Usage:");
  writeNewline(stderr, Indent);
  writeNewline(stderr, Indent);
  printDescription(stderr, TabWidth, Indent, ExecName);
  if (HasOptions)
    printDescriptionContinue(stderr, TabWidth, Indent, " [Options]");
  for (Arg* A : PlacementArgs) {
    printDescriptionContinue(stderr, TabWidth, Indent, " ");
    printDescriptionContinue(stderr, TabWidth, Indent, A->getOptionName());
    if (A->isRepeatable())
      printDescriptionContinue(stderr, TabWidth, Indent, " ...");
  }
  writeNewline(stderr, Indent);
  if (Description != nullptr) {
    writeNewline(stderr, Indent);
    printDescription(stderr, TabWidth, Indent, Description);
    writeChar(stderr, TabWidth, Indent, '.');
    writeNewline(stderr, Indent);
  }
  if (Args.empty())
    return;
  writeNewline(stderr, Indent);
  printDescription(stderr, 0, Indent, "Arguments:");
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
