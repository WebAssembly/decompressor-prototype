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

// Defines a command-line argument parser.

#ifndef DECOMPRESSOR_SRC_UTILS_ARGSPARSE_H
#define DECOMPRESSOR_SRC_UTILS_ARGSPARSE_H

#include "utils/Casting.h"
#include "utils/Defs.h"

#include <string>
#include <set>
#include <vector>

namespace wasm {

namespace utils {

class ArgsParser {
  ArgsParser(const ArgsParser&) = delete;
  ArgsParser& operator=(const ArgsParser&) = delete;

 public:
  enum class State { Bad, Usage, Good };
  enum class ArgKind { Optional, Required };

  class Arg {
    Arg() = delete;
    Arg(const Arg&) = delete;
    Arg& operator=(const Arg&) = delete;

   public:
    void describe(FILE* Out, size_t TabSize) const;
    virtual bool select(charstring OptionValue) = 0;
    virtual int compare(const Arg& A) const;
    char getShortName() const { return ShortName; }
    Arg& setShortName(char Name) {
      ShortName = Name;
      return *this;
    }
    charstring getLongName() const { return LongName; }
    Arg& setLongName(charstring Name) {
      LongName = Name;
      return *this;
    }
    charstring getOptionName() const { return OptionName; }
    Arg& setOptionName(charstring Name) {
      OptionName = Name;
      return *this;
    }
    Arg& setDescription(charstring NewValue) {
      Description = NewValue;
      return *this;
    }
    bool getOptionFound() const { return OptionFound; }
    void setOptionFound(bool Value) { OptionFound = Value; }
    size_t getAddIndex() const { return AddIndex; }
    void setAddIndex(size_t Value) { AddIndex = Value; }
    ArgKind getRtClassId() const { return Kind; }
    static bool implementsClass(ArgKind) { return true; }

    virtual ~Arg() {}

   protected:
    explicit Arg(ArgKind Kind, charstring Description)
        : Kind(Kind),
          ShortName(0),
          LongName(nullptr),
          Description(Description),
          OptionName(nullptr),
          OptionFound(false),
          AddIndex(0) {}
    ArgKind Kind;
    char ShortName;
    charstring LongName;
    charstring Description;
    mutable charstring OptionName;
    bool OptionFound;
    size_t AddIndex;

    virtual void describeDefault(FILE* Out,
                                 size_t TabSize,
                                 size_t& Indent) const;
    virtual void describeOptionName(FILE* Out,
                                    size_t TabSize,
                                    size_t& Indent) const;
  };

  class RequiredArg;

  class OptionalArg : public Arg {
    OptionalArg() = delete;
    OptionalArg(const OptionalArg&) = delete;
    OptionalArg& operator=(const OptionalArg&) = delete;

   public:
    int compare(const Arg& A) const OVERRIDE;
    int compareWithRequired(const RequiredArg& R) const;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Optional; }

    ~OptionalArg() OVERRIDE {}

   protected:
    explicit OptionalArg(charstring Description);
  };

  template <class T>
  class Optional : public OptionalArg {
    Optional() = delete;
    Optional(const Optional&) = delete;
    Optional& operator=(const Optional&) = delete;

   public:
    explicit Optional(T& Value, charstring Description = nullptr)
        : OptionalArg(Description), Value(Value), DefaultValue(Value) {}
    Optional& setDefault(T NewDefault) {
      Value = DefaultValue = NewDefault;
      return *this;
    }
    bool select(charstring NewDefault) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

    ~Optional() OVERRIDE {}

   protected:
    T& Value;
    T DefaultValue;
  };

  class Toggle : public Optional<bool> {
    Toggle() = delete;
    Toggle(const Toggle&) = delete;
    Toggle& operator=(const Toggle&) = delete;

   public:
    explicit Toggle(bool& Value, charstring Description = nullptr)
        : Optional<bool>(Value, Description) {}
    bool select(charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

    ~Toggle() OVERRIDE {}
  };

  class RequiredArg : public Arg {
    RequiredArg() = delete;
    RequiredArg(const RequiredArg&) = delete;
    RequiredArg& operator=(const RequiredArg&) = delete;

   public:
    int compare(const Arg& A) const OVERRIDE;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Required; }

    ~RequiredArg() OVERRIDE {}

   protected:
    RequiredArg(charstring Description) : Arg(ArgKind::Required, Description) {
      // Force a default name.
      OptionName = "ARG";
    }
  };

  template <class T>
  class Required : public RequiredArg {
    Required() = delete;
    Required(const Required&) = delete;
    Required& operator=(const Required&) = delete;

   public:
    explicit Required(T& Value, charstring Description = nullptr)
        : RequiredArg(Description), Value(Value) {}
    bool select(charstring OptionValue) OVERRIDE;

    ~Required() OVERRIDE {}

   private:
    T& Value;
  };

  explicit ArgsParser(const char* Description = nullptr);

  void setExecName(charstring Name) { ExecName = Name; }
  ArgsParser& add(Arg& A);

  State parse(int Argc, charstring Argv[]);
  bool parseShortName(const Arg* A,
                      charstring Argument,
                      charstring& Leftover) const;
  bool parseLongName(const Arg* A,
                     charstring Argument,
                     charstring& Leftover) const;

 private:
  charstring ExecName;
  charstring Description;
  int Argc;
  charstring* Argv;
  bool Help;
  Optional<bool> HelpFlag;
  std::vector<Arg*> Args;
  std::vector<Arg*> ShortArgs;
  std::vector<Arg*> LongArgs;
  std::vector<Arg*> PlacementArgs;
  std::vector<RequiredArg*> RequiredArgs;
  int CurArg;
  size_t CurPlacement;
  State Status;

  void parseNextArg();
  Arg* parseNextShort(charstring Argument, charstring& Leftover);
  Arg* parseNextLong(charstring Argument, charstring& Leftover);
  void showUsage();
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_ARGSPARSE_H
