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

// Implements C++ implementations of algorithms. Used by cast2casm-boot{1,2} and
// cast2casm. The former is to be able to generate boot files while the latter
// is the full blown tool.

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "sexp-parser/Driver.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "stream/FileWriter.h"
#include "stream/ReadCursor.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace {

charstring LocalName = "Local_";
charstring FuncName = "Func_";

bool GenerateEnum = false;
bool GenerateFunction = false;

#if WASM_CAST_BOOT == 2
bool Bootstrap = false;
charstring BootstrapAlg = "Algcasm0x0Boot";
#endif

constexpr size_t WorkBufferSize = 128;
typedef char BufferType[WorkBufferSize];

class CodeGenerator {
  CodeGenerator() = delete;
  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator& operator=(const CodeGenerator&) = delete;

 public:
  CodeGenerator(charstring Filename,
                std::shared_ptr<RawStream> Output,
                std::shared_ptr<SymbolTable> Symtab,
                std::vector<charstring>& Namespaces,
                charstring AlgName)
      : Filename(Filename),
        Output(Output),
        Symtab(Symtab),
        Namespaces(Namespaces),
        AlgName(AlgName),
        ErrorsFound(false),
        NextIndex(1) {}
  ~CodeGenerator() {}
  void generateDeclFile();
  void generateImplFile(bool UseArrayImpl);
  bool foundErrors() const { return ErrorsFound; }
  void setStartPos(std::shared_ptr<ReadCursor> StartPos) { ReadPos = StartPos; }

 private:
  charstring Filename;
  std::shared_ptr<RawStream> Output;
  std::shared_ptr<SymbolTable> Symtab;
  std::shared_ptr<ReadCursor> ReadPos;
  std::vector<charstring>& Namespaces;
  std::vector<const LiteralActionDef*> ActionDefs;
  charstring AlgName;
  bool ErrorsFound;
  size_t NextIndex;

  void puts(charstring Str) { Output->puts(Str); }
  void putc(char Ch) { Output->putc(Ch); }
  void putSymbol(charstring Name, bool Capitalize = true);
  char symbolize(char Ch, bool Capitalize);
#if WASM_CAST_BOOT > 1
  void generateArrayImplFile();
#endif
  void generateFunctionImplFile();
  void generateHeader();
  void generateEnterNamespaces();
  void generateExitNamespaces();
  void collectActionDefs();
  void generatePredefinedEnum();
  void generatePredefinedEnumNames();
  void generatePredefinedNameFcn();
  void generateAlgorithmHeader();
  void generateAlgorithmHeader(charstring AlgName);
  size_t generateNode(const Node* Nd);
  size_t generateNodeFcn(const Node* Nd, std::function<void()> ArgsFn);
  size_t generateSymbol(const Node* Nd);
  size_t generateInteger(const Node* Nd);
  size_t generateNullary(const Node* Nd);
  size_t generateUnary(const Node* Nd);
  size_t generateBinary(const Node* Nd);
  size_t generateTernary(const Node* Nd);
  size_t generateNary(const Node* Nd);
  void generateInt(IntType Value);
  void generateFormat(ValueFormat Format);
  void generateLocal(size_t Index);
  void generateLocalVar(std::string NodeType, size_t Index);
  void generateFunctionName(size_t Index);
  void generateFunctionCall(size_t Index);
  void generateFunctionHeader(std::string NodeType, size_t Index);
  void generateCloseFunctionFooter();
  void generateFunctionFooter();
  void generateCreate(charstring NodeType);
  void generateReturnCreate(charstring NodeType);
  size_t generateBadLocal(const Node* Nd);
  void generateArrayName() {
    puts(AlgName);
    puts("Array");
  }
};

void CodeGenerator::generateInt(IntType Value) {
  BufferType Buffer;
  sprintf(Buffer, "%" PRIuMAX "", uintmax_t(Value));
  puts(Buffer);
}

void CodeGenerator::generateFormat(ValueFormat Format) {
  switch (Format) {
    case ValueFormat::Decimal:
      puts("ValueFormat::Decimal");
      break;
    case ValueFormat::SignedDecimal:
      puts("ValueFormat::SignedDecimal");
      break;
    case ValueFormat::Hexidecimal:
      puts("ValueFormat::Hexidecimal");
      break;
  }
}

void CodeGenerator::generateHeader() {
  puts(
      "// -*- C++ -*- \n"
      "\n"
      "// *** AUTOMATICALLY GENERATED FILE (DO NOT EDIT)! ***\n"
      "\n"
      "// Copyright 2016 WebAssembly Community Group participants\n"
      "//\n"
      "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
      "// you may not use this file except in compliance with the License.\n"
      "// You may obtain a copy of the License at\n"
      "//\n"
      "//     http://www.apache.org/licenses/LICENSE-2.0\n"
      "//\n"
      "// Unless required by applicable law or agreed to in writing, software\n"
      "// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
      "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or "
      "implied.\n"
      "// See the License for the specific language governing permissions and\n"
      "// limitations under the License.\n"
      "\n"
      "// Generated from: \"");
  puts(Filename);
  puts(
      "\"\n"
      "\n"
      "#include \"sexp/Ast.h\"\n"
      "\n"
      "#include <memory>\n"
      "\n");
}

void CodeGenerator::generateEnterNamespaces() {
  for (charstring Name : Namespaces) {
    puts("namespace ");
    puts(Name);
    puts(
        " {\n"
        "\n");
  }
}

void CodeGenerator::generateExitNamespaces() {
  for (std::vector<charstring>::reverse_iterator Iter = Namespaces.rbegin(),
                                                 IterEnd = Namespaces.rend();
       Iter != IterEnd; ++Iter) {
    puts("}  // end of namespace ");
    puts(*Iter);
    puts(
        "\n"
        "\n");
  }
}

void CodeGenerator::putSymbol(charstring Name, bool Capitalize) {
  size_t Len = strlen(Name);
  for (size_t i = 0; i < Len; ++i) {
    char Ch = Name[i];
    switch (Ch) {
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
        putc(i > 0 ? Ch : ((Ch - 'a') + 'A'));
        break;
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
        putc(Ch);
        break;
      case '_':
        puts("__");
        break;
      case '.':
        putc('_');
        break;
      default: {
        BufferType Buffer;
        sprintf(Buffer, "_x%X_", Ch);
        puts(Buffer);
        break;
      }
    }
  }
}

namespace {

IntType getActionDefValue(const LiteralActionDef* Nd) {
  if (const IntegerNode* Num = dyn_cast<IntegerNode>(Nd->getKid(1)))
    return Num->getValue();
  return 0;
}

std::string getActionDefName(const LiteralActionDef* Nd) {
  if (const Symbol* Sym = dyn_cast<Symbol>(Nd->getKid(0)))
    return Sym->getName();
  return std::string("???");
}

bool compareLtActionDefs(const LiteralActionDef* N1,
                         const LiteralActionDef* N2) {
  IntType V1 = getActionDefValue(N1);
  IntType V2 = getActionDefValue(N2);
  if (V1 < V2)
    return true;
  if (V1 > V2)
    return false;
  return getActionDefName(N1) < getActionDefName(N2);
}

}  // end of anonymous namespace

void CodeGenerator::collectActionDefs() {
  if (!ActionDefs.empty())
    return;
  SymbolTable::ActionDefSet DefSet;
  Symtab->collectActionDefs(DefSet);
  for (const LiteralActionDef* Def : DefSet)
    ActionDefs.push_back(Def);
  std::sort(ActionDefs.begin(), ActionDefs.end(), compareLtActionDefs);
}

void CodeGenerator::generatePredefinedEnum() {
  collectActionDefs();
  puts("enum class Predefined");
  puts(AlgName);
  puts(" : uint32_t {\n");
  bool IsFirst = true;
  for (const LiteralActionDef* Def : ActionDefs) {
    if (IsFirst)
      IsFirst = false;
    else
      puts(",\n");
    puts("  ");
    putSymbol(getActionDefName(Def).c_str());
    BufferType Buffer;
    sprintf(Buffer, " = %" PRIuMAX "", uintmax_t(getActionDefValue(Def)));
    puts(Buffer);
  }
  puts(
      "\n"
      "};\n"
      "\n"
      "charstring getName(Predefined");
  puts(AlgName);
  puts(
      " Value);\n"
      "\n");
}

void CodeGenerator::generatePredefinedEnumNames() {
  collectActionDefs();
  puts(
      "struct {\n"
      "  Predefined");
  puts(AlgName);
  puts(
      " Value;\n"
      "  charstring Name;\n"
      "} PredefinedNames[] {\n");
  bool IsFirst = true;
  for (const LiteralActionDef* Def : ActionDefs) {
    if (IsFirst)
      IsFirst = false;
    else
      puts(",\n");
    puts("  {Predefined");
    puts(AlgName);
    puts("::");
    putSymbol(getActionDefName(Def).c_str());
    puts(", \"");
    puts(getActionDefName(Def).c_str());
    puts("\"}");
  }
  puts(
      "\n"
      "};\n"
      "\n");
}

void CodeGenerator::generatePredefinedNameFcn() {
  puts("charstring getName(Predefined");
  puts(AlgName);
  puts(
      " Value) {\n"
      "  for (size_t i = 0; i < size(PredefinedNames); ++i) {\n"
      "    if (PredefinedNames[i].Value == Value) \n"
      "      return PredefinedNames[i].Name;\n"
      "  }\n"
      "  return getName(PredefinedSymbol::Unknown);\n"
      "}\n"
      "\n");
}

void CodeGenerator::generateAlgorithmHeader() {
  generateAlgorithmHeader(AlgName);
}

void CodeGenerator::generateAlgorithmHeader(charstring AlgName) {
  puts("std::shared_ptr<filt::SymbolTable> get");
  puts(AlgName);
  puts("Symtab()");
}

size_t CodeGenerator::generateBadLocal(const Node* Nd) {
  TextWriter Writer;
  fprintf(stderr, "Unrecognized: ");
  Writer.writeAbbrev(stderr, Nd);
  size_t Index = NextIndex++;
  ErrorsFound = true;
  generateLocalVar("Node", Index);
  puts("nullptr;\n");
  return Index;
}

void CodeGenerator::generateLocal(size_t Index) {
  puts(LocalName);
  generateInt(Index);
}

void CodeGenerator::generateLocalVar(std::string NodeType, size_t Index) {
  puts("  ");
  puts(NodeType.c_str());
  puts("* ");
  generateLocal(Index);
  puts(" = ");
}

void CodeGenerator::generateFunctionName(size_t Index) {
  puts(FuncName);
  generateInt(Index);
}

void CodeGenerator::generateFunctionCall(size_t Index) {
  generateFunctionName(Index);
  puts("(Symtab)");
}

void CodeGenerator::generateFunctionHeader(std::string NodeType, size_t Index) {
  puts(NodeType.c_str());
  puts("* ");
  generateFunctionName(Index);
  puts("(SymbolTable* Symtab) {\n");
}

void CodeGenerator::generateFunctionFooter() {
  puts(
      "}\n"
      "\n");
}

void CodeGenerator::generateCloseFunctionFooter() {
  puts(");\n");
  generateFunctionFooter();
}

void CodeGenerator::generateCreate(charstring NodeType) {
  puts("Symtab->create<");
  puts(NodeType);
  puts(">(");
}

void CodeGenerator::generateReturnCreate(charstring NodeType) {
  puts("  return ");
  generateCreate(NodeType);
}

size_t CodeGenerator::generateNodeFcn(const Node* Nd,
                                      std::function<void()> ArgsFn) {
  size_t Index = NextIndex++;
  charstring NodeType = Nd->getNodeName();
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  ArgsFn();
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateSymbol(const Node* Nd) {
  assert(isa<Symbol>(Nd));
  const Symbol* Sym = cast<Symbol>(Nd);
  size_t Index = NextIndex++;
  generateFunctionHeader("Symbol", Index);
  puts("  return Symtab->getOrCreateSymbol(\"");
  for (auto& Ch : Sym->getName())
    putc(Ch);
  putc('"');
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateInteger(const Node* Nd) {
  return generateNodeFcn(Nd, [&]() {
    assert(isa<IntegerNode>(Nd));
    const IntegerNode* IntNd = cast<IntegerNode>(Nd);
    generateInt(IntNd->getValue());
    puts(", ");
    generateFormat(IntNd->getFormat());
  });
}

size_t CodeGenerator::generateNullary(const Node* Nd) {
  assert(Nd->getNumKids() == 0);
  return generateNodeFcn(Nd, []() { return; });
}

size_t CodeGenerator::generateUnary(const Node* Nd) {
  assert(Nd->getNumKids() == 1);
  size_t Kid1 = generateNode(Nd->getKid(0));
  return generateNodeFcn(Nd, [&]() { generateFunctionCall(Kid1); });
}

size_t CodeGenerator::generateBinary(const Node* Nd) {
  assert(Nd->getNumKids() == 2);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  return generateNodeFcn(Nd, [&]() {
    generateFunctionCall(Kid1);
    puts(", ");
    generateFunctionCall(Kid2);
  });
}

size_t CodeGenerator::generateTernary(const Node* Nd) {
  assert(Nd->getNumKids() == 3);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  size_t Kid3 = generateNode(Nd->getKid(2));
  return generateNodeFcn(Nd, [&]() {
    generateFunctionCall(Kid1);
    puts(", ");
    generateFunctionCall(Kid2);
    puts(", ");
    generateFunctionCall(Kid3);
  });
}

size_t CodeGenerator::generateNary(const Node* Nd) {
  std::vector<size_t> Kids;
  charstring NodeType = Nd->getNodeName();
  for (int i = 0; i < Nd->getNumKids(); ++i)
    Kids.push_back(generateNode(Nd->getKid(i)));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateLocalVar(NodeType, Index);
  generateCreate(NodeType);
  puts(");\n");
  for (size_t KidIndex : Kids) {
    puts("  ");
    generateLocal(Index);
    puts("->append(");
    generateFunctionCall(KidIndex);
    puts(");\n");
  }
  puts("  return ");
  generateLocal(Index);
  puts(";\n");
  generateFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNode(const Node* Nd) {
  if (Nd == nullptr)
    return generateBadLocal(Nd);

  switch (Nd->getType()) {
    case NodeType::NO_SUCH_NODETYPE:
    case NodeType::BinaryEvalBits:
      return generateBadLocal(Nd);
    case NodeType::BinaryAccept: {
      return generateNodeFcn(Nd, [&] {
        const BinaryAccept* AcceptNd = cast<BinaryAccept>(Nd);
        generateInt(AcceptNd->getValue());
        puts(", ");
        generateInt(AcceptNd->getNumBits());
      });
    }
    case NodeType::BinaryEval:
      return generateUnary(Nd);
    case NodeType::Opcode:
      return generateNary(Nd);
    case NodeType::Symbol:
      return generateSymbol(Nd);

#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return generateNullary(Nd);
      AST_NULLARYNODE_TABLE
#undef X

#define X(NAME)        \
  case NodeType::NAME: \
    return generateNullary(Nd);
      AST_CACHEDNODE_TABLE
#undef X

#define X(NAME, BASE, VALUE, FORMAT, DECLS, INIT) \
  case NodeType::NAME:                            \
    return generateNullary(Nd);
      AST_LITERAL_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return generateUnary(Nd);
      AST_UNARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return generateBinary(Nd);
      AST_BINARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return generateTernary(Nd);
      AST_TERNARYNODE_TABLE
#undef X

#define X(NAME, BASE, DECLS, INIT) \
  case NodeType::NAME:             \
    return generateNary(Nd);
      AST_NARYNODE_TABLE AST_SELECTNODE_TABLE
#undef X

#define X(NAME, FORMAT, DEFAULT, MERGE, BASE, DECLS, INIT) \
  case NodeType::NAME:                                     \
    return generateInteger(Nd);
          AST_INTEGERNODE_TABLE
#undef X
  }
  WASM_RETURN_UNREACHABLE(0);
}

void CodeGenerator::generateDeclFile() {
  generateHeader();
  generateEnterNamespaces();
  if (GenerateEnum)
    generatePredefinedEnum();
  if (GenerateFunction) {
    generateAlgorithmHeader();
    puts(";\n\n");
  }
  generateExitNamespaces();
}

#if WASM_CAST_BOOT > 1
void CodeGenerator::generateArrayImplFile() {
  puts(
      "namespace {\n"
      "\n");
  BufferType Buffer;
  constexpr size_t BytesPerLine = 15;
  puts("static const uint8_t ");
  generateArrayName();
  puts("[] = {\n");
  while (!ReadPos->atEof()) {
    uint8_t Byte = ReadPos->readByte();
    size_t Address = ReadPos->getCurAddress();
    if (Address > 0 && Address % BytesPerLine == 0)
      putc('\n');
    bool IsPrintable = std::isalnum(Byte);
    switch (Byte) {
      case ' ':
      case '!':
      case '@':
      case '#':
      case '$':
      case '%':
      case '^':
      case '&':
      case '*':
      case '(':
      case ')':
      case '_':
      case '-':
      case '+':
      case '=':
      case '{':
      case '[':
      case '}':
      case ']':
      case '|':
      case ':':
      case ';':
      case '<':
      case ',':
      case '>':
      case '.':
      case '?':
      case '/':
        IsPrintable = true;
      default:
        break;
    }
    if (IsPrintable)
      sprintf(Buffer, " '%c'", Byte);
    else
      sprintf(Buffer, " %u", Byte);
    puts(Buffer);
    if (!ReadPos->atEof())
      putc(',');
  }
  puts(
      "};\n"
      "\n"
      "}  // end of anonymous namespace\n"
      "\n");

#if WASM_CAST_BOOT == 2
  if (Bootstrap) {
    generateAlgorithmHeader(BootstrapAlg);
    puts(
        ";\n"
        "\n");
  }
#endif
  generateAlgorithmHeader();
  puts(
      " {\n"
      "  static std::shared_ptr<SymbolTable> Symtable;\n"
      "  if (Symtable)\n"
      "    return Symtable;\n"
      "  auto ArrayInput = std::make_shared<ArrayReader>(\n"
      "    ");
  generateArrayName();
  puts(", size(");
  generateArrayName();
  puts(
      "));\n"
      "  auto Input = std::make_shared<ReadBackedQueue>(ArrayInput);\n"
      "  CasmReader Reader;\n");
#if WASM_CAST_BOOT == 2
  if (Bootstrap) {
    puts("  std::shared_ptr<SymbolTable> BootSymtab = get");
    puts(BootstrapAlg);
    puts(
        "Symtab();\n"
        "  Reader.readBinary(Input, BootSymtab, BootSymtab);\n");
  } else {
    puts("  Reader.readBinary(Input);\n");
  };
#else
  puts("  Reader.readBinary(Input);\n");
#endif
  puts(
      "  if (Reader.hasErrors()) {\n"
      "    fatal(\"Malformed builtin algorithm: ");
  puts(AlgName);
  puts(
      "\");\n"
      "  }\n"
      "  Symtable = Reader.getReadSymtab();\n"
      "  return Symtable;\n");
  generateFunctionFooter();
}
#endif

void CodeGenerator::generateFunctionImplFile() {
  puts(
      "namespace {\n"
      "\n");
  size_t Index = generateNode(Symtab->getAlgorithm());
  puts(
      "}  // end of anonymous namespace\n"
      "\n");
  generateAlgorithmHeader();
  puts(
      " {\n"
      "  static std::shared_ptr<SymbolTable> Symtable;\n"
      "  if (Symtable)\n"
      "    return Symtable;\n"
      "  Symtable = std::make_shared<SymbolTable>();\n"
      "  SymbolTable* Symtab = Symtable.get();\n"
      "  Symtab->setAlgorithm(");
  generateFunctionCall(Index);
  puts(
      ");\n"
      "  Symtab->install();\n"
      "  SymbolTable::registerAlgorithm(Symtable);\n"
      "  return Symtable;\n");
  generateFunctionFooter();
}

void CodeGenerator::generateImplFile(bool UseArrayImpl) {
  generateHeader();
  if (UseArrayImpl)
    puts(
        "#include \"casm/CasmReader.h\"\n"
        "#include \"stream/ArrayReader.h\"\n"
        "#include \"stream/ReadBackedQueue.h\"\n"
        "\n"
        "#include <cassert>\n"
        "\n");
  generateEnterNamespaces();
  puts(
      "using namespace wasm::filt;\n"
      "\n");
  // Note: We don't know the include path for the enum, so just repeat
  // generating it.
  if (GenerateEnum) {
    generatePredefinedEnum();
    puts(
        "namespace {\n"
        "\n");
    generatePredefinedEnumNames();
    puts(
        "} // end of anonymous namespace\n"
        "\n");
    generatePredefinedNameFcn();
  }
  if (GenerateFunction) {
#if WASM_CAST_BOOT > 1
    if (UseArrayImpl)
      generateArrayImplFile();
    else
#endif
      generateFunctionImplFile();
  }
  generateExitNamespaces();
}

std::shared_ptr<SymbolTable> readCasmFile(
    const char* Filename,
    bool Verbose,
    bool TraceLexer,
    bool TraceParser,
    std::shared_ptr<SymbolTable> EnclosingScope) {
  bool HasErrors = false;
  std::shared_ptr<SymbolTable> Symtab;
#if WASM_CAST_BOOT < 3
  Symtab = std::make_shared<SymbolTable>(EnclosingScope);
  Driver Parser(Symtab);
  Parser.setTraceLexing(TraceLexer);
  Parser.setTraceParsing(TraceParser);
  Parser.setTraceFilesParsed(Verbose);
  HasErrors = !Parser.parse(Filename);
#else
  CasmReader Reader;
  if (Verbose)
    fprintf(stderr, "Reading: %s\n", Filename);
  Reader.setTraceRead(TraceParser)
      .setTraceLexer(TraceLexer)
      .readTextOrBinary(Filename, EnclosingScope);
  HasErrors = Reader.hasErrors();
  if (!HasErrors)
    Symtab = Reader.getReadSymtab();
#endif
  if (HasErrors)
    Symtab = SymbolTable::SharedPtr();
  return Symtab;
}

bool install(SymbolTable::SharedPtr Symtab, bool Display, const char* Title) {
  if (Display) {
    fprintf(stdout, "After stripping %s:\n", Title);
    TextWriter Writer;
    Writer.write(stdout, Symtab->getAlgorithm());
  }
  bool Success = Symtab->install();
  if (!Success)
    fprintf(stderr, "Problems stripping %s, can't continue.\n", Title);
  return Success;
}

}  // end of anonymous namespace

}  // end of namespace wasm;

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::utils;

int main(int Argc, charstring Argv[]) {
  std::vector<charstring> AlgorithmFilenames;
  std::vector<charstring> InputFilenames;
  charstring OutputFilename = "-";
  charstring AlgName = nullptr;
  std::set<std::string> KeepActions;
  bool DisplayParsedInput = false;
  bool DisplayStrippedInput = false;
  bool DisplayStripAll = false;
  bool ShowInternalStructure = false;
  bool StripActions = false;
  bool StripSymbolicActions = false;
  bool StripAll = false;
  bool StripLiterals = false;
  bool StripLiteralDefs = false;
  bool StripLiteralUses = false;
  bool TraceAlgorithm = false;
  bool TraceLexer = false;
  bool TraceParser = false;
  bool Verbose = false;
  bool HeaderFile = false;

#if WASM_CAST_BOOT > 1
  bool BitCompress = true;
  bool MinimizeBlockSize = false;
  bool TraceFlatten = false;
  bool TraceWrite = false;
  bool TraceTree = false;
  bool UseArrayImpl = false;
  bool ValidateWhileWriting = false;
#endif

  {
    ArgsParser Args("Converts compression algorithm from text to binary");

    ArgsParser::Optional<bool> ExpectFailFlag(ExpectExitFail);
    Args.add(ExpectFailFlag.setDefault(false)
                 .setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success"));

    ArgsParser::Optional<bool> GenerateEnumFlag(GenerateEnum);
    Args.add(GenerateEnumFlag.setLongName("enum").setDescription(
        "Generate C++ source code implementing an enum for actions"));

    ArgsParser::Optional<bool> GenerateFunctionFlag(GenerateFunction);
    Args.add(GenerateFunctionFlag.setLongName("function")
                 .setDescription(
                     "Generate C++ source code implementing algorithm creation "
                     "function"));

    ArgsParser::Optional<charstring> AlgNameFlag(AlgName);
    Args.add(
        AlgNameFlag.setShortName('f')
            .setLongName("name")
            .setOptionName("NAME")
            .setDescription("The name prefix used to generate the name of C++ "
                            "generated enum and/or function"));

#if WASM_CAST_BOOT == 2
    ArgsParser::Toggle BootstrapFlag(Bootstrap);
    Args.add(BootstrapFlag.setShortName('b').setLongName("boot").setDescription(
        "When true, the array implementation will be read "
        "using bootstrap algorithm getAlgcasm0x0BootSymtab()"));
#endif

    ArgsParser::Optional<bool> HeaderFileFlag(HeaderFile);
    Args.add(HeaderFileFlag.setLongName("header").setDescription(
        "Generate header version of c++ source instead "
        "of implementatoin file (only applies when "
        "'--function Name' is specified)"));

    ArgsParser::RequiredVector<charstring> InputFilenamesFlag(InputFilenames);
    Args.add(InputFilenamesFlag.setOptionName("INPUT").setDescription(
        "Text file(s) to convert to binary. If repeated, the "
        "last file is the algorithm converted to binary. "
        "Previous files define enclosing scopes for the "
        "converted algorithm"));

    ArgsParser::OptionalSet<std::string> KeepActionsFlag(KeepActions);
    Args.add(
        KeepActionsFlag.setLongName("keep")
            .setOptionName("ACTION")
            .setDescription("Don't strip callbacks on ACTION from the input"));

    ArgsParser::Optional<charstring> OutputFlag(OutputFilename);
    Args.add(OutputFlag.setShortName('o')
                 .setLongName("output")
                 .setOptionName("OUTPUT")
                 .setDescription("Generated binary file"));

    ArgsParser::Toggle DisplayParsedInputFlag(DisplayParsedInput);
    Args.add(DisplayParsedInputFlag.setShortName('d')
                 .setLongName("display=input")
                 .setDescription("Display parsed cast text"));

    ArgsParser::Toggle DisplayStrippedInputFlag(DisplayStrippedInput);
    Args.add(DisplayStrippedInputFlag.setLongName("display=stripped")
                 .setDescription("Display parsed cast text after all strip "
                                 "operations"));

    ArgsParser::Toggle DisplayStripAllFlag(DisplayStripAll);
    Args.add(DisplayStripAllFlag.setLongName("display=strip-all")
                 .setDescription("Display parsed cast text after each strip "
                                 "operation"));

    ArgsParser::Toggle ShowInternalStructureFlag(ShowInternalStructure);
    Args.add(ShowInternalStructureFlag.setShortName('i')
                 .setLongName("internal")
                 .setDescription("Show internal structure when displaying "
                                 "parsed text"));

    ArgsParser::Toggle StripActionsFlag(StripActions);
    Args.add(StripActionsFlag.setLongName("strip-actions")
                 .setDescription("Remove callback actions from input"));

    ArgsParser::Toggle StripSymbolicActionsFlag(StripSymbolicActions);
    Args.add(StripSymbolicActionsFlag.setLongName("strip-symbolic-actions")
                 .setDescription("Substitute action enumerated value for all "
                                 "action symbolic names"));

    ArgsParser::Toggle StripAllFlag(StripAll);
    Args.add(StripAllFlag.setShortName('s').setLongName("strip").setDescription(
        "Apply all strip actions to input"));

    ArgsParser::Toggle StripLiteralsFlag(StripLiterals);
    Args.add(StripLiteralsFlag.setLongName("strip-literals")
                 .setDescription(
                     "Replace literal uses with their definition, then "
                     "remove unreferenced literal definitions from the input"));

    ArgsParser::Toggle StripLiteralDefsFlag(StripLiteralDefs);
    Args.add(StripLiteralDefsFlag.setLongName("strip-literal-defs")
                 .setDescription("Remove unreferenced literal definitions from "
                                 "the input"));

    ArgsParser::Toggle StripLiteralUsesFlag(StripLiteralUses);
    Args.add(StripLiteralUsesFlag.setLongName("strip-literal-uses")
                 .setDescription("Replace literal uses with their defintion"));

    ArgsParser::Optional<bool> TraceLexerFlag(TraceLexer);
    Args.add(
        TraceLexerFlag.setLongName("verbose=lexer")
            .setDescription("Show lexing of algorithm (defined by option -a)"));

    ArgsParser::Toggle TraceParserFlag(TraceParser);
    Args.add(TraceParserFlag.setLongName("verbose=parser")
                 .setDescription(
                     "Show parsing of algorithm (defined by option -a)"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(
        VerboseFlag.setShortName('v').setLongName("verbose").setDescription(
            "Show progress and tree written to binary file"));

    ArgsParser::Optional<bool> TraceAlgorithmFlag(TraceAlgorithm);
    Args.add(
        TraceAlgorithmFlag.setLongName("verbose=algorithm")
            .setDescription("Show algorithm used to generate compressed file"));

#if WASM_CAST_BOOT > 1

    ArgsParser::OptionalVector<charstring> AlgorithmFilenamesFlag(
        AlgorithmFilenames);
    Args.add(AlgorithmFilenamesFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Instead of using the default casm algorithm to generate "
                     "the casm binary file, use the aglorithm defined by "
                     "ALGORITHM(s). If repeated, each file defines the "
                     "enclosing scope for the next ALGORITHM file"));

    ArgsParser::Optional<bool> BitCompressFlag(BitCompress);
    Args.add(BitCompressFlag.setLongName("bit-compress")
                 .setDescription(
                     "Perform bit compresssion on binary opcode expressions"));

    ArgsParser::Toggle MinimizeBlockFlag(MinimizeBlockSize);
    Args.add(MinimizeBlockFlag.setDefault(true)
                 .setShortName('m')
                 .setLongName("minimize")
                 .setDescription("Minimize size in binary file "
                                 "(note: runs slower)"));

    ArgsParser::Optional<bool> TraceFlattenFlag(TraceFlatten);
    Args.add(TraceFlattenFlag.setLongName("verbose=flatten")
                 .setDescription("Show how algorithms are flattened"));

    ArgsParser::Optional<bool> TraceWriteFlag(TraceWrite);
    Args.add(TraceWriteFlag.setLongName("verbose=write")
                 .setDescription("Show how binary file is encoded"));

    ArgsParser::Optional<bool> TraceTreeFlag(TraceTree);
    Args.add(TraceTreeFlag.setLongName("verbose=tree")
                 .setDescription("Show tree being written while writing "
                                 "(implies --verbose=write)"));

    ArgsParser::Optional<bool> UseArrayImplFlag(UseArrayImpl);
    Args.add(UseArrayImplFlag.setLongName("array").setDescription(
        "Internally implement function NAME() using an "
        "array implementation, rather than the default that "
        "uses direct code"));

    ArgsParser::Toggle ValidateWhileWritingFlag(ValidateWhileWriting);
    Args.add(ValidateWhileWritingFlag.setLongName("validate")
                 .setDescription(
                     "While writing, validate that it is readable. Useful "
                     "when writing new algorithms to parse algorithms."));

#endif

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }

    // Be sure to update implications!

    if (StripAll) {
      StripActions = true;
      StripSymbolicActions = false;
      StripLiterals = true;
    }

    if (InputFilenames.empty())
      InputFilenames.push_back("-");

#if WASM_CAST_BOOT > 1
    if (TraceTree)
      TraceWrite = true;
    // TODO(karlschimpf) Extend ArgsParser to be able to return option
    // name so that we don't have hard-coded dependency.
    if (UseArrayImpl && AlgName == nullptr) {
      fprintf(stderr, "Option --array can't be used without option --name\n");
      return exit_status(EXIT_FAILURE);
    }

    if (UseArrayImpl && HeaderFile) {
      fprintf(stderr, "Opition --array can't be used with option --header\n");
      return exit_status(EXIT_FAILURE);
    }
#endif
  }

  std::shared_ptr<SymbolTable> InputSymtab;
  charstring InputFilename = "-";
  for (charstring Filename : InputFilenames) {
    InputFilename = Filename;
    InputSymtab =
        readCasmFile(Filename, Verbose, TraceLexer, TraceParser, InputSymtab);
    if (!InputSymtab || !InputSymtab->install()) {
      fprintf(stderr, "Unable to parse: %s\n", InputFilename);
      return exit_status(EXIT_FAILURE);
    }
  }

  if (DisplayParsedInput) {
    if (Verbose)
      fprintf(stdout, "Parsed input:\n");
    InputSymtab->describe(stdout, ShowInternalStructure);
    if (!(DisplayStrippedInput || DisplayStripAll))
      return exit_status(EXIT_SUCCESS);
  }

  if (StripActions) {
    InputSymtab->stripCallbacksExcept(KeepActions);
    if (!install(InputSymtab, DisplayStripAll, "actions"))
      return exit_status(EXIT_FAILURE);
  }

  if (StripSymbolicActions) {
    InputSymtab->stripSymbolicCallbacks();
    if (!install(InputSymtab, DisplayStripAll, "symbolic actions"))
      return exit_status(EXIT_FAILURE);
  }

  if (StripLiteralUses) {
    InputSymtab->stripLiteralUses();
    if (!install(InputSymtab, DisplayStripAll, "literal uses"))
      return exit_status(EXIT_FAILURE);
  }

  if (StripLiteralDefs) {
    InputSymtab->stripLiteralDefs();
    if (!install(InputSymtab, DisplayStripAll, "literal definitions"))
      return exit_status(EXIT_FAILURE);
  }

  if (StripLiterals) {
    InputSymtab->stripLiterals();
    if (!install(InputSymtab, DisplayStripAll, "literals"))
      return exit_status(EXIT_FAILURE);
  }

  if (DisplayParsedInput || DisplayStrippedInput || DisplayStripAll) {
    if (DisplayStrippedInput && !DisplayStripAll) {
      if (Verbose)
        fprintf(stdout, "Stripped input:\n");
      InputSymtab->describe(stdout, ShowInternalStructure);
    }
    return exit_status(EXIT_SUCCESS);
  }

  std::shared_ptr<SymbolTable> AlgSymtab;
  for (charstring Filename : AlgorithmFilenames) {
    AlgSymtab =
        readCasmFile(Filename, Verbose, TraceLexer, TraceParser, AlgSymtab);
    if (!AlgSymtab || !AlgSymtab->install()) {
      fprintf(stderr, "Problems reading file: %s\n", Filename);
      return exit_status(EXIT_FAILURE);
    }
  }

#if WASM_CAST_BOOT > 1
  if (AlgorithmFilenames.empty()) {
    if (Verbose)
      fprintf(stderr, "Using prebuilt casm algorithm\n");
    AlgSymtab = WASM_CASM_GET_SYMTAB();
  }

  if (TraceAlgorithm) {
    fprintf(stderr, "Algorithm:\n");
    TextWriter Writer;
    Writer.write(stderr, AlgSymtab);
  }
#endif

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Opening file: %s\n", OutputFilename);
  auto Output = std::make_shared<FileWriter>(OutputFilename);
  if (Output->hasErrors()) {
    fprintf(stderr, "Problems opening output file: %s", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }

  std::shared_ptr<Queue> OutputStream;
  std::shared_ptr<ReadCursor> OutputStartPos;
  if (AlgName != nullptr) {
#if WASM_CAST_BOOT > 1
    if (UseArrayImpl) {
      OutputStream = std::make_shared<Queue>();
      OutputStartPos =
          std::make_shared<ReadCursor>(StreamType::Byte, OutputStream);
    }
#endif
  } else {
    OutputStream = std::make_shared<WriteBackedQueue>(Output);
  }

#if WASM_CAST_BOOT > 1
  if (OutputStream) {
    // Generate binary stream
    CasmWriter Writer;
    Writer.setTraceWriter(TraceWrite)
        .setTraceFlatten(TraceFlatten)
        .setTraceTree(TraceTree)
        .setMinimizeBlockSize(MinimizeBlockSize)
        .setBitCompress(BitCompress)
        .setValidateWhileWriting(ValidateWhileWriting)
        .writeBinary(InputSymtab, OutputStream, AlgSymtab);
    if (Writer.hasErrors()) {
      fprintf(stderr, "Problems writing: %s\n", OutputFilename);
      return exit_status(EXIT_FAILURE);
    }
  }
#endif

  if (AlgName == nullptr)
    return exit_status(EXIT_SUCCESS);

  // Generate C++ code.
  std::vector<charstring> Namespaces;
  Namespaces.push_back("wasm");
  Namespaces.push_back("decode");
  CodeGenerator Generator(InputFilename, Output, InputSymtab, Namespaces,
                          AlgName);
  if (HeaderFile)
    Generator.generateDeclFile();
  else {
#if WASM_CAST_BOOT > 1
    if (UseArrayImpl)
      Generator.setStartPos(OutputStartPos);
#else
    bool UseArrayImpl = false;
#endif
    Generator.generateImplFile(UseArrayImpl);
  }
  if (Generator.foundErrors()) {
    fprintf(stderr, "Unable to generate valid C++ source!\n");
    return exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
