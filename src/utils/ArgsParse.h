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
    virtual void describe(FILE* Out, size_t TabSize) const = 0;
    virtual void describeDefault(FILE* out,
                                 size_t TabSize,
                                 size_t& Indent) const;
    virtual bool select(charstring OptionValue) = 0;
    virtual int compare(const Arg& A) const;
    charstring getOptionName() const { return OptionName; }
    Arg& setOptionName(charstring Name) {
      OptionName = Name;
      return *this;
    }
    Arg& setDescription(charstring NewValue) {
      Description = NewValue;
      return *this;
    }
    size_t getAddIndex() const { return AddIndex; }
    void setAddIndex(size_t Value) { AddIndex = Value; }
    ArgKind getRtClassId() const { return Kind; }
    static bool implementsClass(ArgKind) { return true; }

   protected:
    explicit Arg(ArgKind Kind, charstring Description)
        : Kind(Kind),
          Description(Description),
          OptionName(nullptr),
          AddIndex(0) {}
    ArgKind Kind;
    charstring Description;
    mutable charstring OptionName;
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
    char getShortName() const { return ShortName; }
    Optional& setShortName(char Name) {
      ShortName = Name;
      return *this;
    }
    charstring getLongName() const { return LongName; }
    Optional& setLongName(charstring Name) {
      LongName = Name;
      return *this;
    }
    virtual void describe(FILE* Out, size_t TabSize) const OVERRIDE;
    int compare(const Arg& A) const OVERRIDE;
    int compareWithRequired(const Required& R) const;

   protected:
    char ShortName;
    charstring LongName;
    explicit Optional(charstring Description);
  };

  class Bool : public Optional {
    Bool() = delete;
    Bool(const Bool&) = delete;
    Bool& operator=(const Bool&) = delete;

   public:
    explicit Bool(bool& Value, charstring Description = nullptr)
        : Optional(Description), Value(Value), DefaultValue(Value) {}
    ~Bool() OVERRIDE;
    Bool& setDefault(bool NewDefault) {
      Value = DefaultValue = NewDefault;
      return *this;
    }
    bool select(charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   private:
    bool& Value;
    bool DefaultValue;
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
    virtual void describe(FILE* Out, size_t TabSize) const OVERRIDE;
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
  bool parseShortName(const Optional* A,
                      charstring Argument,
                      charstring& Leftover) const;
  bool parseLongName(const Optional* A,
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
  std::vector<Optional*> OptionalsShort;
  std::vector<Optional*> OptionalsLong;
  std::vector<Required*> Requireds;
  int CurArg;
  size_t CurRequired;
  State Status;

  void parseNextArg();
  Optional* parseNextShort(charstring Argument, charstring& Leftover);
  Optional* parseNextLong(charstring Argument, charstring& Leftover);
  void showUsage();
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_ARGSPARSE_H
