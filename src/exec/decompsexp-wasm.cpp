/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "binary/BinaryWriter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadCursor.h"
#include "stream/StreamReader.h"
#include "stream/StreamWriter.h"
#include "stream/WriteBackedQueue.h"
#include "utils/Defs.h"

#include <cctype>
#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace wasm;
using namespace wasm::filt;
using namespace wasm::decode;

namespace {

bool UseFileStreams = true;
const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    if (UseFileStreams)
      return std::make_shared<FdWriter>(STDOUT_FILENO, false);
    return std::make_shared<StreamWriter>(std::cout);
  }
  if (UseFileStreams)
    return std::make_shared<FileWriter>(OutputFilename);
  return std::make_shared<FstreamWriter>(OutputFilename);
}

#if BOOTSTRAP
void generateArrayImpl(std::shared_ptr<ReadCursor> ReadPos) {
  auto Output = getOutput();
  static const char* Preamble =
      "// -*- C++ -*- */\n"
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
      "#include \"sexp/defaults.h\"\n"
      "\n"
      "static const uint8_t WasmDefaults[] = {\n";
  for (size_t i = 0, len = strlen(Preamble); i < len; ++i)
    Output->putc(Preamble[i]);
  char Buffer[256];
  while (!ReadPos->atEof()) {
    uint8_t Byte = ReadPos->readByte();
    sprintf(Buffer, "   %u", Byte);
    for (size_t i = 0, len = std::strlen(Buffer); i < len; ++i)
      Output->putc(Buffer[i]);
    if (!ReadPos->atEof())
      Output->putc(',');
    Output->putc('\n');
  }
  static const char* Postamble =
      "};\n"
      "\n"
      "namespace wasm {\n"
      "namespace decode {\n"
      "const uint8_t *getWasmDefaultsBuffer() { return WasmDefaults; }\n"
      "size_t getWasmDefaultsBufferSize() { return size(WasmDefaults); }\n"
      "} // end of namespace decode\n"
      "} // end of namespace wasm\n";
  for (size_t i = 0, len = strlen(Postamble); i < len; ++i)
    Output->putc(Postamble[i]);
  Output->freeze();
}
#endif

void usage(const char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "  Convert WASM filter s-expressions to WASM binary.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --expect-fail\t\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\t\tPrint this usage message.\n");
  fprintf(stderr, "  -i File\t\tFile of s-expressions ('-' implies stdin).\n");
  fprintf(stderr, "  -m\t\t\tMinimize block sizes in output stream.\n");
  fprintf(stderr, "  -o File\t\tGenerated WASM binary ('-' implies stdout).\n");
  fprintf(stderr, "  -s\t\t\tUse C++ streams instead of C file descriptors.\n");
  if (isDebug()) {
    fprintf(stderr,
            "  -v | --verbose\t"
            "Show progress (can be repeated for more detail).\n");
    fprintf(stderr,
            "\t\t\t-v       : Show progress of writing out wasm file.\n");
    fprintf(stderr, "\t\t\t-v -v    : Add tracing of parsing s-expressions.\n");
    fprintf(stderr, "\t\t\t-v -v -v : Add tracing of lexing s-expressions.\n");
  }
}

}  // end of anonymous namespace

int main(int Argc, char* Argv[]) {
  fprintf(stderr, "BOOTSTRAP = %u\n", BOOTSTRAP);
  int Verbose = 0;
  bool MinimizeBlockSize = false;
  bool InputSpecified = false;
  bool OutputSpecified = false;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail")) {
      ExpectExitFail = true;
    } else if (Argv[i] == std::string("-h") ||
               Argv[i] == std::string("--help")) {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
    } else if (Argv[i] == std::string("-i")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -i option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      if (InputSpecified) {
        fprintf(stderr, "-i <input> option can't be repeated\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      InputFilename = Argv[i];
      InputSpecified = true;
    } else if (Argv[i] == std::string("-m")) {
      MinimizeBlockSize = true;
    } else if (Argv[i] == std::string("-o")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      if (OutputSpecified) {
        fprintf(stderr, "-i <output> option can't be repeated\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
      OutputSpecified = true;
    } else if (Argv[i] == std::string("-s")) {
      UseFileStreams = true;
    } else if (isDebug() && (Argv[i] == std::string("-v") ||
                             (Argv[i] == std::string("--verbose")))) {
      ++Verbose;
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  auto Symtab = std::make_shared<SymbolTable>();
  Driver Parser(Symtab);
  Parser.setTraceParsing(Verbose >= 2);
  Parser.setTraceLexing(Verbose >= 3);
  if (!Parser.parse(InputFilename)) {
    fprintf(stderr, "Unable to parse s-expressions: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }
#if BOOTSTRAP
  auto TmpStream = std::make_shared<Queue>();
  auto ReadPos = std::make_shared<ReadCursor>(StreamType::Byte, TmpStream);
  BinaryWriter Writer(TmpStream, Symtab);
#else
  BinaryWriter Writer(std::make_shared<WriteBackedQueue>(getOutput()), Symtab);
#endif
  Writer.setTraceProgress(Verbose >= 1);
  Writer.setMinimizeBlockSize(MinimizeBlockSize);
  Writer.writePreamble();
  Writer.writeFile(wasm::dyn_cast<FileNode>(Parser.getParsedAst()));
  Writer.freezeEof();
#if BOOTSTRAP
  generateArrayImpl(ReadPos);
#endif
  return exit_status(EXIT_SUCCESS);
}
