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
  typedef const char* charstring;

  class Arg {
    Arg() = delete;
    Arg(const Arg&) = delete;
    Arg& operator=(const Arg&) = delete;

   public:
    virtual ~Arg();
    void describe(FILE* Out, size_t TabSize) const;
    virtual void describeDefault(FILE* out,
                                 size_t TabSize,
                                 size_t& Indent) const;
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
  };

  class Required;

  class Optional : public Arg {
    Optional() = delete;
    Optional(const Optional&) = delete;
    Optional& operator=(const Optional&) = delete;

   public:
    ~Optional() OVERRIDE;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Optional; }
    int compare(const Arg& A) const OVERRIDE;
    int compareWithRequired(const Required& R) const;

   protected:
    explicit Optional(charstring Description);
  };

  class Bool : public Optional {
    Bool() = delete;
    Bool(const Bool&) = delete;
    Bool& operator=(const Bool&) = delete;

   public:
    explicit Bool(bool& Value, charstring Description = nullptr)
        : Optional(Description),
          Value(Value),
          DefaultValue(Value),
          Toggle(false) {}
    ~Bool() OVERRIDE;
    Bool& setDefault(bool NewDefault) {
      Value = DefaultValue = NewDefault;
      return *this;
    }
    Bool& setToggle(bool Value) {
      Toggle = Value;
      return *this;
    }
    bool select(charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   private:
    bool& Value;
    bool DefaultValue;
    bool Toggle;
  };

  class OptionalCharstring : public Optional {
    OptionalCharstring() = delete;
    OptionalCharstring(const OptionalCharstring&) = delete;
    OptionalCharstring& operator=(const OptionalCharstring&) = delete;

   public:
    explicit OptionalCharstring(charstring& Value,
                                charstring Description = nullptr)
        : Optional(Description), Value(Value), DefaultValue(Value) {}
    ~OptionalCharstring() OVERRIDE;
    OptionalCharstring& setDefault(charstring NewDefault) {
      Value = DefaultValue = NewDefault;
      return *this;
    }
    bool select(charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   private:
    charstring& Value;
    charstring DefaultValue;
  };

  class Required : public Arg {
    Required() = delete;
    Required(const Required&) = delete;
    Required& operator=(const Required&) = delete;

   public:
    ~Required() OVERRIDE {}

    int compare(const Arg& A) const OVERRIDE;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Required; }

   protected:
    Required(charstring Description) : Arg(ArgKind::Required, Description) {
      // Force a default name.
      OptionName = "ARG";
    }
  };

  class RequiredCharstring : public Required {
    RequiredCharstring() = delete;
    RequiredCharstring(const RequiredCharstring&) = delete;
    RequiredCharstring& operator=(const RequiredCharstring&) = delete;

   public:
    explicit RequiredCharstring(charstring& Value,
                                charstring Description = nullptr)
        : Required(Description), Value(Value) {}
    ~RequiredCharstring() OVERRIDE;
    bool select(charstring OptionValue) OVERRIDE;

   private:
    charstring& Value;
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
  Bool HelpFlag;
  std::vector<Arg*> Args;
  std::vector<Arg*> Shorts;
  std::vector<Arg*> Longs;
  std::vector<Arg*> Placements;
  std::vector<Required*> Requireds;
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
