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

  static charstring getName(State S);
  static charstring getName(ArgKind K);

  class Arg {
    Arg() = delete;
    Arg(const Arg&) = delete;
    Arg& operator=(const Arg&) = delete;

   public:
    void describe(FILE* Out, size_t TabSize) const;
    virtual bool select(ArgsParser* Parser, charstring OptionValue) = 0;
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

    bool isRepeatable() const { return IsRepeatable; }
    bool getOptionFound() const { return OptionFound; }
    virtual void setOptionFound();
    virtual void setPlacementFound(size_t& CurPlacement);
    bool validOptionValue(ArgsParser* Parser, charstring OptionValue);
    size_t getAddIndex() const { return AddIndex; }
    void setAddIndex(size_t Value) { AddIndex = Value; }
    ArgKind getRtClassId() const { return Kind; }
    static bool implementsClass(ArgKind) { return true; }

    virtual ~Arg() {}

   protected:
    explicit Arg(ArgKind Kind)
        : Kind(Kind),
          ShortName(0),
          LongName(nullptr),
          Description(nullptr),
          OptionName(nullptr),
          OptionFound(false),
          AddIndex(0),
          IsRepeatable(false) {}
    ArgKind Kind;
    char ShortName;
    charstring LongName;
    charstring Description;
    mutable charstring OptionName;
    bool OptionFound;
    size_t AddIndex;
    bool IsRepeatable;

    virtual void describeDefault(FILE* Out,
                                 size_t TabSize,
                                 size_t& Indent) const;
    virtual void describeOptionName(FILE* Out,
                                    size_t TabSize,
                                    size_t& Indent) const;

    // The following are support writing routines for parser arguments.
    static void endLineIfOver(FILE* Out, const size_t TabSize, size_t& Indent) {
      ArgsParser::endLineIfOver(Out, TabSize, Indent);
    }
    static void indentTo(FILE* Out, const size_t TabSize, size_t& Indent) {
      ArgsParser::indentTo(Out, TabSize, Indent);
    }
    static void writeNewline(FILE* Out, size_t& Indent) {
      ArgsParser::writeNewline(Out, Indent);
    }
    static void writeChar(FILE* Out,
                          const size_t TabSize,
                          size_t& Indent,
                          char Ch) {
      ArgsParser::writeChar(Out, TabSize, Indent, Ch);
    }
    static void writeChunk(FILE* Out,
                           const size_t TabSize,
                           size_t& Indent,
                           charstring String,
                           size_t Chunk) {
      ArgsParser::writeChunk(Out, TabSize, Indent, String, Chunk);
    }
    static void writeCharstring(FILE* Out,
                                const size_t TabSize,
                                size_t& Indent,
                                charstring String) {
      ArgsParser::writeCharstring(Out, TabSize, Indent, String);
    }
    static void writeSize_t(FILE* Out,
                            const size_t TabSize,
                            size_t& Indent,
                            size_t Value) {
      ArgsParser::writeSize_t(Out, TabSize, Indent, Value);
    }
    static void printDescriptionContinue(FILE* Out,
                                         const size_t TabSize,
                                         size_t& Indent,
                                         charstring Description) {
      ArgsParser::printDescriptionContinue(Out, TabSize, Indent, Description);
    }
    static void printDescription(FILE* Out,
                                 size_t TabSize,
                                 size_t& Indent,
                                 charstring Description) {
      ArgsParser::printDescription(Out, TabSize, Indent, Description);
    }
  };

  class RequiredArg;

  class OptionalArg : public Arg {
    OptionalArg(const OptionalArg&) = delete;
    OptionalArg& operator=(const OptionalArg&) = delete;

   public:
    int compare(const Arg& A) const OVERRIDE;
    int compareWithRequired(const RequiredArg& R) const;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Optional; }

    ~OptionalArg() OVERRIDE {}

   protected:
    OptionalArg();
  };

  template <class T>
  class Optional : public OptionalArg {
    Optional() = delete;
    Optional(const Optional&) = delete;
    Optional& operator=(const Optional&) = delete;

   public:
    explicit Optional(T& Value)
        : OptionalArg(), Value(Value), DefaultValue(Value) {}
    Optional& setDefault(T NewDefault) {
      Value = DefaultValue = NewDefault;
      return *this;
    }
    bool select(ArgsParser* Parser, charstring NewDefault) OVERRIDE;
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
    explicit Toggle(bool& Value) : Optional<bool>(Value) {}
    bool select(ArgsParser* Parser, charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

    ~Toggle() OVERRIDE {}
  };

  template <class T>
  class SetValue : public Optional<T> {
    SetValue() = delete;
    SetValue(const SetValue&) = delete;
    SetValue& operator=(const SetValue&) = delete;

   public:
    SetValue(T& Value, T SelectValue, charstring Description = nullptr)
        : Optional<T>(Value, Description) {}

    bool select(ArgsParser* Parser, charstring OptionValue) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

    ~SetValue() OVERRIDE {}

   protected:
    T SelectValue;
  };

  template <class T>
  class OptionalSet : public OptionalArg {
    OptionalSet() = delete;
    OptionalSet(const OptionalSet&) = delete;
    OptionalSet& operator=(const OptionalSet&) = delete;

   public:
    typedef std::set<T> set_type;
    explicit OptionalSet(set_type& Values) : OptionalArg(), Values(Values) {
      IsRepeatable = true;
    }

    bool select(ArgsParser* Parser, charstring Add) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   protected:
    set_type& Values;
  };

  template <class T>
  class OptionalVector : public OptionalArg {
    OptionalVector() = delete;
    OptionalVector(const OptionalVector&) = delete;
    OptionalVector& operator=(const OptionalVector&) = delete;

   public:
    typedef std::vector<T> vector_type;
    explicit OptionalVector(vector_type& Values)
        : OptionalArg(), Values(Values) {
      IsRepeatable = true;
    }

    bool select(ArgsParser* Parser, charstring Add) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   protected:
    vector_type& Values;
  };

  class RequiredArg : public Arg {
    RequiredArg(const RequiredArg&) = delete;
    RequiredArg& operator=(const RequiredArg&) = delete;

   public:
    int compare(const Arg& A) const OVERRIDE;
    static bool implementsClass(ArgKind Id) { return Id == ArgKind::Required; }

    ~RequiredArg() OVERRIDE {}

   protected:
    RequiredArg() : Arg(ArgKind::Required) {
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
    explicit Required(T& Value) : RequiredArg(), Value(Value) {}
    bool select(ArgsParser* Parser, charstring OptionValue) OVERRIDE;

    ~Required() OVERRIDE {}

   private:
    T& Value;
  };

  template <class T>
  class RequiredVector : public RequiredArg {
    RequiredVector() = delete;
    RequiredVector(const RequiredVector&) = delete;
    RequiredVector& operator=(const RequiredVector&) = delete;

   public:
    typedef std::vector<T> vector_type;
    explicit RequiredVector(vector_type& Values, size_t MinSize = 1)
        : RequiredArg(), Values(Values), MinSize(MinSize) {
      IsRepeatable = true;
      assert(MinSize > 0);
    }

    void setOptionFound() OVERRIDE;
    void setPlacementFound(size_t& CurPlacement) OVERRIDE;
    bool select(ArgsParser* Parser, charstring Add) OVERRIDE;
    void describeDefault(FILE* Out,
                         size_t TabSize,
                         size_t& Indent) const OVERRIDE;

   protected:
    vector_type& Values;
    size_t MinSize;
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

  void setTraceProgress(bool Value) { TraceProgress = Value; }
  bool getTraceProgress() const { return TraceProgress; }

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
  bool TraceProgress;

  void parseNextArg();
  Arg* parseNextShort(charstring Argument, charstring& Leftover);
  Arg* parseNextLong(charstring Argument, charstring& Leftover);
  void showUsage();

  // Records that there was an error, and then returns stream to print
  // error message to.
  FILE* error();

  friend class Arg;

  // The following are support writing routines for parser arguments.
  static const size_t TabWidth;
  static const size_t MaxLine;

  static void endLineIfOver(FILE* Out, const size_t TabSize, size_t& Indent);
  static void indentTo(FILE* Out, const size_t TabSize, size_t& Indent);
  static void writeNewline(FILE* Out, size_t& Indent);
  static void writeChar(FILE* Out,
                        const size_t TabSize,
                        size_t& Indent,
                        char Ch);
  static void writeChunk(FILE* Out,
                         const size_t TabSize,
                         size_t& Indent,
                         charstring String,
                         size_t Chunk);
  static void writeCharstring(FILE* Out,
                              const size_t TabSize,
                              size_t& Indent,
                              charstring String);
  static void writeSize_t(FILE* Out,
                          const size_t TabSize,
                          size_t& Indent,
                          size_t Value);
  static void printDescriptionContinue(FILE* Out,
                                       const size_t TabSize,
                                       size_t& Indent,
                                       charstring Description);
  static void printDescription(FILE* Out,
                               size_t TabSize,
                               size_t& Indent,
                               charstring Description);
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_ARGSPARSE_H
