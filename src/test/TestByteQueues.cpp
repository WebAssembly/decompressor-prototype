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

// Tests reading/writing using byte queues.

#include "stream/FileReader.h"
#include "stream/FileWriter.h"
#include "stream/ReadBackedQueue.h"
#include "stream/ReadCursor.h"
#include "stream/WriteBackedQueue.h"
#include "stream/WriteCursor.h"

#include <cstring>
#include <unistd.h>

#include <iostream>

using namespace wasm::decode;

namespace {

const char* InputFilename = "-";
const char* OutputFilename = "-";

std::shared_ptr<RawStream> getInput() {
  if (InputFilename == std::string("-"))
    return std::make_shared<FdReader>(STDIN_FILENO, false);
  return std::make_shared<FileReader>(InputFilename);
}

std::shared_ptr<RawStream> getOutput() {
  if (OutputFilename == std::string("-")) {
    return std::make_shared<FdWriter>(STDOUT_FILENO, false);
  }
  return std::make_shared<FileWriter>(OutputFilename);
}

void usage(char* AppName) {
  fprintf(stderr, "usage: %s [options]\n", AppName);
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c N\t\tRead N bytes (i.e. chunksize) at a time\n");
  fprintf(stderr, "  --expect-fail\tSucceed on failure/fail on success\n");
  fprintf(stderr, "  -h\t\tShow usage\n");
  fprintf(stderr, "  -i NAME\tRead from input file NAME ('-' implies stdin)\n");
  fprintf(stderr,
          "  -o NAME\tWrite to output file NAME ('-' implies stdout)\n");
}

}  // end of anonymous namespace

int main(int Argc, char* Argv[]) {
  int BufSize = 1;
  static constexpr int MaxBufSize = 4096;
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i] == std::string("--expect-fail"))
      ExpectExitFail = true;
    else if (Argv[i] == std::string("-i")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -i option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      InputFilename = Argv[i];
    } else if (Argv[i] == std::string("-o")) {
      if (++i >= Argc) {
        fprintf(stderr, "No file specified after -o option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      OutputFilename = Argv[i];
    } else if (Argv[i] == std::string("-c")) {
      if (++i >= Argc) {
        fprintf(stderr, "No byte count after -c option\n");
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      int Size = atoi(Argv[i]);
      if (Size < 1) {
        fprintf(stderr, "Chunk size %d must be > 0\n", Size);
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      if (Size > MaxBufSize) {
        fprintf(stderr, "Chunk size %d can't exceed %d\n", Size, MaxBufSize);
        usage(Argv[0]);
        return exit_status(EXIT_FAILURE);
      }
      BufSize = Size;
    } else if (Argv[i] == std::string("-h") ||
               (Argv[i] == std::string("--help"))) {
      usage(Argv[0]);
      return exit_status(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", Argv[i]);
      usage(Argv[0]);
      return exit_status(EXIT_FAILURE);
    }
  }
  auto Input = std::make_shared<ReadBackedQueue>(getInput());
  auto Output = std::make_shared<WriteBackedQueue>(getOutput());
  // uint8_t Buffer[MaxBufSize];
  size_t Address = 0;
  ReadCursor ReadPos(Input);
  WriteCursor WritePos(Output);
  while (Address < Input->currentSize()) {
    size_t ReadBytesAvailable = Input->readFromPage(Address, BufSize, ReadPos);
    if (ReadBytesAvailable == 0) {
      WritePos.setMaxAddress(Address);
      break;
    }
    size_t NextAddress = Address + ReadBytesAvailable;
    while (ReadBytesAvailable) {
      size_t WriteBytesAvailable =
          Output->writeToPage(Address, ReadBytesAvailable, WritePos);
      if (WriteBytesAvailable == 0) {
        fprintf(stderr, "Unable to write address %d, returned zero bytes\n",
                int(Address));
        return exit_status(EXIT_FAILURE);
      }
      if (WriteBytesAvailable > ReadBytesAvailable) {
        fprintf(stderr, "Unable to write address %d, returned %d extra bytes\n",
                int(Address), int(WriteBytesAvailable - ReadBytesAvailable));
        return exit_status(EXIT_FAILURE);
      }
      // TODO(karlschimpf): Fix API to allow range copies.
      for (size_t i = 0; i < WriteBytesAvailable; ++i)
        WritePos.writeByte(ReadPos.readByte());
      ReadBytesAvailable -= WriteBytesAvailable;
    }
    Address = NextAddress;
  }
  return exit_status(EXIT_SUCCESS);
}
