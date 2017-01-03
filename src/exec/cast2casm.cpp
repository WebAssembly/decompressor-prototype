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

// Converts textual algorithm into binary file form.

#include "utils/Defs.h"

#if WASM_BOOT == 0
#include "algorithms/casm0x0.h"
#endif

#include "sexp/CasmReader.h"
#include "sexp/CasmWriter.h"
#include "stream/FileWriter.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

#include <cstdlib>

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::utils;

namespace {

static charstring LocalName = "Local_";
static charstring FuncName = "Func_";

class CodeGenerator {
  CodeGenerator() = delete;
  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator& operator=(const CodeGenerator&) = delete;

 public:
  CodeGenerator(charstring Filename,
                std::shared_ptr<RawStream> Output,
                std::shared_ptr<SymbolTable> Symtab,
                std::vector<charstring>& Namespaces,
                charstring FunctionName)
      : Filename(Filename),
        Output(Output),
        Symtab(Symtab),
        Namespaces(Namespaces),
        FunctionName(FunctionName),
        FoundErrors(false),
        NextIndex(1) {}
  ~CodeGenerator() {}
  bool generateDeclFile();
  bool generateImplFile();

 private:
  charstring Filename;
  std::shared_ptr<RawStream> Output;
  std::shared_ptr<SymbolTable> Symtab;
  std::vector<charstring>& Namespaces;
  charstring FunctionName;
  bool FoundErrors;
  size_t NextIndex;

  void generateHeader();
  void generateEnterNamespaces();
  void generateExitNamespaces();
  void generateAlgorithmHeader();
  size_t generateNode(const Node* Nd);
  size_t generateSymbol(const SymbolNode* Sym);
  size_t generateIntegerNode(charstring NodeType, const IntegerNode* Nd);
  size_t generateNullaryNode(charstring NodeType, const Node* Nd);
  size_t generateUnaryNode(charstring NodeType, const Node* Nd);
  size_t generateBinaryNode(charstring NodeType, const Node* Nd);
  size_t generateTernaryNode(charstring NodeType, const Node* Nd);
  size_t generateNaryNode(charstring NodeName, const Node* Nd);
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
  size_t generateBadLocal();
};

void CodeGenerator::generateInt(IntType Value) {
  char Buffer[40];
  sprintf(Buffer, "%" PRIuMAX "", uintmax_t(Value));
  Output->puts(Buffer);
}

void CodeGenerator::generateFormat(ValueFormat Format) {
  switch (Format) {
    case ValueFormat::Decimal:
      Output->puts("ValueFormat::Decimal");
      break;
    case ValueFormat::SignedDecimal:
      Output->puts("ValueFormat::SignedDecimal");
      break;
    case ValueFormat::Hexidecimal:
      Output->puts("ValueFormat::Hexidecimal");
      break;
  }
}

void CodeGenerator::generateHeader() {
  Output->puts(
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
  Output->puts(Filename);
  Output->puts(
      "\"\n"
      "\n"
      "#include \"sexp/Ast.h\"\n"
      "\n"
      "#include <memory>\n"
      "\n");
}

void CodeGenerator::generateEnterNamespaces() {
  for (charstring Name : Namespaces) {
    Output->puts("namespace ");
    Output->puts(Name);
    Output->puts(
        " {\n"
        "\n");
  }
}

void CodeGenerator::generateExitNamespaces() {
  for (std::vector<charstring>::reverse_iterator Iter = Namespaces.rbegin(),
                                                 IterEnd = Namespaces.rend();
       Iter != IterEnd; ++Iter) {
    Output->puts("}  // end of namespace ");
    Output->puts(*Iter);
    Output->puts(
        "\n"
        "\n");
  }
}

void CodeGenerator::generateAlgorithmHeader() {
  Output->puts("void ");
  Output->puts(FunctionName);
  Output->puts("(std::shared_ptr<filt::SymbolTable> Symtable)");
}

size_t CodeGenerator::generateBadLocal() {
  size_t Index = NextIndex++;
  FoundErrors = true;
  generateLocalVar("Node", Index);
  Output->puts("nullptr;\n");
  return Index;
}

void CodeGenerator::generateLocal(size_t Index) {
  Output->puts(LocalName);
  generateInt(Index);
}

void CodeGenerator::generateLocalVar(std::string NodeType, size_t Index) {
  Output->puts("  ");
  Output->puts(NodeType.c_str());
  Output->puts("* ");
  generateLocal(Index);
  Output->puts(" = ");
}

void CodeGenerator::generateFunctionName(size_t Index) {
  Output->puts(FuncName);
  generateInt(Index);
}

void CodeGenerator::generateFunctionCall(size_t Index) {
  generateFunctionName(Index);
  Output->puts("(Symtab)");
}

void CodeGenerator::generateFunctionHeader(std::string NodeType, size_t Index) {
  Output->puts(NodeType.c_str());
  Output->puts("* ");
  generateFunctionName(Index);
  Output->puts("(SymbolTable* Symtab) {\n");
}

void CodeGenerator::generateFunctionFooter() {
  Output->puts(
      "}\n"
      "\n");
}

void CodeGenerator::generateCloseFunctionFooter() {
  Output->puts(");\n");
  generateFunctionFooter();
}

void CodeGenerator::generateCreate(charstring NodeType) {
  Output->puts("Symtab->create<");
  Output->puts(NodeType);
  Output->puts(">(");
}

void CodeGenerator::generateReturnCreate(charstring NodeType) {
  Output->puts("  return ");
  generateCreate(NodeType);
}

size_t CodeGenerator::generateSymbol(const SymbolNode* Sym) {
  size_t Index = NextIndex++;
  generateFunctionHeader("SymbolNode", Index);
  Output->puts("  return Symtab->getSymbolDefinition(\"");
  for (auto& Ch : Sym->getName())
    Output->putc(Ch);
  Output->putc('"');
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateIntegerNode(charstring NodeName,
                                          const IntegerNode* Nd) {
  size_t Index = NextIndex++;
  std::string NodeType(NodeName);
  NodeType.append("Node");
  generateFunctionHeader(NodeType, Index);
  Output->puts("  return Symtab->get");
  Output->puts(NodeName);
  Output->puts("Definition(");
  generateInt(Nd->getValue());
  Output->puts(", ");
  generateFormat(Nd->getFormat());
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNullaryNode(charstring NodeType, const Node* Nd) {
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateUnaryNode(charstring NodeType, const Node* Nd) {
  size_t Index = NextIndex++;
  assert(Nd->getNumKids() == 1);
  size_t Kid1 = generateNode(Nd->getKid(0));
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateBinaryNode(charstring NodeType, const Node* Nd) {
  assert(Nd->getNumKids() == 2);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  Output->puts(", ");
  generateFunctionCall(Kid2);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateTernaryNode(charstring NodeType, const Node* Nd) {
  assert(Nd->getNumKids() == 3);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  size_t Kid3 = generateNode(Nd->getKid(2));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  Output->puts(", ");
  generateFunctionCall(Kid2);
  Output->puts(", ");
  generateFunctionCall(Kid3);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNaryNode(charstring NodeType, const Node* Nd) {
  std::vector<size_t> Kids;
  for (int i = 0; i < Nd->getNumKids(); ++i)
    Kids.push_back(generateNode(Nd->getKid(i)));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateLocalVar(NodeType, Index);
  generateCreate(NodeType);
  Output->puts(");\n");
  for (size_t KidIndex : Kids) {
    Output->puts("  ");
    generateLocal(Index);
    Output->puts("->append(");
    generateFunctionCall(KidIndex);
    Output->puts(");\n");
  }
  Output->puts("  return ");
  generateLocal(Index);
  Output->puts(";\n");
  generateFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNode(const Node* Nd) {
  if (Nd == nullptr)
    return generateBadLocal();

  switch (Nd->getType()) {
    default:
      return generateBadLocal();
    case OpAnd:
      return generateBinaryNode("AndNode", Nd);
    case OpBitwiseAnd:
      return generateBinaryNode("BitwiseAndNode", Nd);
    case OpBitwiseNegate:
      return generateUnaryNode("BitwiseNegateNode", Nd);
    case OpBitwiseOr:
      return generateBinaryNode("BitwiseOrNode", Nd);
    case OpBitwiseXor:
      return generateBinaryNode("BitwiseXorNode", Nd);
    case OpBlock:
      return generateUnaryNode("BlockNode", Nd);
    case OpCallback:
      return generateUnaryNode("CallbackNode", Nd);
    case OpCase:
      return generateBinaryNode("CaseNode", Nd);
    case OpConvert:
      return generateTernaryNode("ConvertNode", Nd);
    case OpDefine:
      return generateNaryNode("DefineNode", Nd);
    case OpError:
      return generateNullaryNode("ErrorNode", Nd);
    case OpEval:
      return generateNaryNode("EvalNode", Nd);
    case OpFile:
      return generateTernaryNode("FileNode", Nd);
    case OpFileHeader:
      return generateNaryNode("FileHeaderNode", Nd);
    case OpFilter:
      return generateNaryNode("FilterNode", Nd);
    case OpIfThen:
      return generateBinaryNode("IfThenNode", Nd);
    case OpIfThenElse:
      return generateTernaryNode("IfThenElseNode", Nd);
    case OpI32Const:
      return generateIntegerNode("I32Const", cast<I32ConstNode>(Nd));
    case OpI64Const:
      return generateIntegerNode("I64Const", cast<I64ConstNode>(Nd));
    case OpLastRead:
      return generateNullaryNode("LastReadNode", Nd);
    case OpLastSymbolIs:
      return generateUnaryNode("LastSymbolIsNode", Nd);
    case OpLiteralDef:
      return generateBinaryNode("LiteralDefNode", Nd);
    case OpLiteralUse:
      return generateUnaryNode("LiteralUseNode", Nd);
    case OpLocal:
      return generateIntegerNode("Local", cast<LocalNode>(Nd));
    case OpLocals:
      return generateIntegerNode("Locals", cast<LocalNode>(Nd));
    case OpLoop:
      return generateBinaryNode("LoopNode", Nd);
    case OpLoopUnbounded:
      return generateUnaryNode("LoopUnboundedNode", Nd);
    case OpMap:
      return generateNaryNode("MapNode", Nd);
    case OpNot:
      return generateUnaryNode("NotNode", Nd);
    case OpOpcode:
      return generateNaryNode("OpcodeNode", Nd);
    case OpOr:
      return generateBinaryNode("OrNode", Nd);
    case OpParam:
      return generateIntegerNode("Param", cast<ParamsNode>(Nd));
    case OpParams:
      return generateIntegerNode("Params", cast<ParamsNode>(Nd));
    case OpPeek:
      return generateUnaryNode("PeekNode", Nd);
    case OpRead:
      return generateUnaryNode("ReadNode", Nd);
    case OpRename:
      return generateBinaryNode("RenameNode", Nd);
    case OpSection:
      return generateNaryNode("SectionNode", Nd);
    case OpSequence:
      return generateNaryNode("SequenceNode", Nd);
    case OpSet:
      return generateBinaryNode("SetNode", Nd);
    case OpStream:
      return generateBinaryNode("StreamNode", Nd);
    case OpSymbol:
      return generateSymbol(cast<SymbolNode>(Nd));
    case OpSwitch:
      return generateNaryNode("SwitchNode", Nd);
    case OpUint8:
      return generateIntegerNode("Uint8", cast<Uint8Node>(Nd));
    case OpUint32:
      return generateIntegerNode("Uint32", cast<Uint32Node>(Nd));
    case OpUint64:
      return generateIntegerNode("Uint64", cast<Uint64Node>(Nd));
    case OpUndefine:
      return generateUnaryNode("UndefineNode", Nd);
    case OpU8Const:
      return generateIntegerNode("U8Const", cast<U8ConstNode>(Nd));
    case OpU32Const:
      return generateIntegerNode("U32Const", cast<U32ConstNode>(Nd));
    case OpU64Const:
      return generateIntegerNode("U64Const", cast<U64ConstNode>(Nd));
    case OpVarint32:
      return generateIntegerNode("Varint32", cast<Varint32Node>(Nd));
    case OpVarint64:
      return generateIntegerNode("Varint64", cast<Varint64Node>(Nd));
    case OpVaruint32:
      return generateIntegerNode("Varuint32", cast<Varuint32Node>(Nd));
    case OpVaruint64:
      return generateIntegerNode("Varuint64", cast<Varuint64Node>(Nd));
    case OpVoid:
      return generateNullaryNode("VoidNode", Nd);
    case OpWrite:
      return generateNaryNode("WriteNode", Nd);
  }
  WASM_RETURN_UNREACHABLE(0);
}

bool CodeGenerator::generateDeclFile() {
  generateHeader();
  generateEnterNamespaces();
  generateAlgorithmHeader();
  Output->puts(";\n\n");
  generateExitNamespaces();
  return FoundErrors;
}

bool CodeGenerator::generateImplFile() {
  generateHeader();
  generateEnterNamespaces();
  Output->puts(
      "using namespace wasm::filt;\n"
      "\n"
      "namespace {\n"
      "\n");
  size_t Index = generateNode(Symtab->getInstalledRoot());
  Output->puts(
      "}  // end of anonymous namespace\n"
      "\n");
  generateAlgorithmHeader();
  Output->puts(
      " {\n"
      "  SymbolTable* Symtab = Symtable.get();\n"
      "  Symtab->install(");
  generateFunctionCall(Index);
  generateCloseFunctionFooter();
  generateExitNamespaces();
  return FoundErrors;
}

}  // end of anonymous namespace

int main(int Argc, charstring Argv[]) {
  charstring InputFilename = "-";
  charstring OutputFilename = "-";
  charstring AlgorithmFilename = nullptr;
  bool MinimizeBlockSize = false;
  bool Verbose = false;
  bool TraceFlatten = false;
  bool TraceLexer = false;
  bool TraceParser = false;
  bool TraceWrite = false;
  bool TraceTree = false;
  charstring FunctionName = nullptr;
  bool HeaderFile;
  {
    ArgsParser Args("Converts compression algorithm from text to binary");

#if WASM_BOOT
    ArgsParser::Required<charstring>
#else
    ArgsParser::Optional<charstring>
#endif
        AlgorithmFlag(AlgorithmFilename);
    Args.add(AlgorithmFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Use algorithm in ALGORITHM file "
                     "to parse text file"));

    ArgsParser::Optional<bool> ExpectFailFlag(ExpectExitFail);
    Args.add(ExpectFailFlag.setDefault(false)
                 .setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success"));

    ArgsParser::Toggle MinimizeBlockFlag(MinimizeBlockSize);
    Args.add(MinimizeBlockFlag.setDefault(true)
                 .setShortName('m')
                 .setLongName("minimize")
                 .setDescription(
                     "Minimize size in binary file "
                     "(note: runs slower)"));

    ArgsParser::Required<charstring> InputFlag(InputFilename);
    Args.add(InputFlag.setOptionName("INPUT")
                 .setDescription("Text file to convert to binary"));

    ArgsParser::Optional<charstring> OutputFlag(OutputFilename);
    Args.add(OutputFlag.setShortName('o')
                 .setLongName("output")
                 .setOptionName("OUTPUT")
                 .setDescription("Generated binary file"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(
        VerboseFlag.setShortName('v').setLongName("verbose").setDescription(
            "Show progress and tree written to binary file"));

    ArgsParser::Optional<bool> TraceFlattenFlag(TraceFlatten);
    Args.add(TraceFlattenFlag.setLongName("verbose=flatten")
                 .setDescription("Show how algorithms are flattened"));

    ArgsParser::Optional<bool> TraceWriteFlag(TraceWrite);
    Args.add(TraceWriteFlag.setLongName("verbose=write")
                 .setDescription("Show how binary file is encoded"));

    ArgsParser::Optional<bool> TraceTreeFlag(TraceTree);
    Args.add(TraceTreeFlag.setLongName("verbose=tree")
                 .setDescription(
                     "Show tree being written while writing "
                     "(implies --verbose=write)"));

    ArgsParser::Optional<bool> TraceParserFlag(TraceParser);
    Args.add(TraceParserFlag.setLongName("verbose=parser")
                 .setDescription(
                     "Show parsing of algorithm (defined by option -a)"));

    ArgsParser::Optional<bool> TraceLexerFlag(TraceLexer);
    Args.add(
        TraceLexerFlag.setLongName("verbose=lexer")
            .setDescription("Show lexing of algorithm (defined by option -a)"));

    ArgsParser::Optional<charstring> FunctionNameFlag(FunctionName);
    Args.add(FunctionNameFlag.setShortName('f')
                 .setLongName("function")
                 .setOptionName("Name")
                 .setDescription(
                     "Generate c++ source code to implement an array "
                     "containing the binary encoding, with accessors "
                     "Name() and NameSize()"));

    ArgsParser::Optional<bool> HeaderFileFlag(HeaderFile);
    Args.add(HeaderFileFlag.setLongName("header").setDescription(
        "Generate header version of c++ source instead "
        "of implementatoin file (only applies when "
        "'--function Name' is specified)"));

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
    if (TraceTree)
      TraceWrite = true;

    assert(!(WASM_BOOT && AlgorithmFilename == nullptr));
  }

  if (Verbose)
    fprintf(stderr, "Reading input: %s\n", InputFilename);
  std::shared_ptr<SymbolTable> InputSymtab;
  {
    CasmReader Reader;
    Reader.setTraceRead(TraceParser)
        .setTraceLexer(TraceLexer)
        .setTraceTree(Verbose)
        .readText(InputFilename);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Unable to parse: %s\n", InputFilename);
      return exit_status(EXIT_FAILURE);
    }
    InputSymtab = Reader.getReadSymtab();
  }

  if (Verbose) {
    if (AlgorithmFilename)
      fprintf(stderr, "Reading algorithms file: %s\n", AlgorithmFilename);
    else
      fprintf(stderr, "Using prebuilt casm algorithm\n");
  }
  std::shared_ptr<SymbolTable> AlgSymtab;
  if (AlgorithmFilename) {
    CasmReader Reader;
    Reader.setTraceRead(TraceParser)
        .setTraceLexer(TraceLexer)
        .setTraceTree(Verbose)
        .readText(AlgorithmFilename);
    if (Reader.hasErrors()) {
      fprintf(stderr, "Problems reading file: %s\n", InputFilename);
      return exit_status(EXIT_FAILURE);
    }
    AlgSymtab = Reader.getReadSymtab();
#if WASM_BOOT == 0
  } else {
    AlgSymtab = std::make_shared<SymbolTable>();
    install_Algcasm0x0(AlgSymtab);
#endif
  }

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Opening file: %s\n", OutputFilename);
  auto Output = std::make_shared<FileWriter>(OutputFilename);
  if (Output->hasErrors()) {
    fprintf(stderr, "Problems opening output file: %s", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }

  if (FunctionName != nullptr) {
    // Generate C++ code.
    std::vector<charstring> Namespaces;
    Namespaces.push_back("wasm");
    Namespaces.push_back("decode");
    CodeGenerator Generator(InputFilename, Output, InputSymtab, Namespaces,
                            FunctionName);
    if (HeaderFile ? Generator.generateDeclFile()
                   : Generator.generateImplFile()) {
      fprintf(stderr, "Unable to generate valid C++ source!");
      return exit_status(EXIT_FAILURE);
    }
    return exit_status(EXIT_SUCCESS);
  }

  // If reached, geneate binary CASM file.
  std::shared_ptr<decode::Queue> OutputStream =
      std::make_shared<WriteBackedQueue>(Output);

  CasmWriter Writer;
  Writer.setTraceWriter(TraceWrite)
      .setTraceFlatten(TraceFlatten)
      .setTraceTree(TraceTree)
      .setMinimizeBlockSize(MinimizeBlockSize);
  Writer.writeBinary(InputSymtab, OutputStream, AlgSymtab);
  if (Writer.hasErrors()) {
    fprintf(stderr, "Problems writing: %s\n", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
