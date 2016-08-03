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

/* Implements common definitions used by the decompressor. */

#include "Defs.h"
#include <stdio.h>
#include <cstdlib>

namespace wasm {

namespace decode {

bool ExpectExitFail = false;

int exit_status(int Status) {
  if (!ExpectExitFail)
    return Status;
  return Status ? EXIT_SUCCESS : EXIT_FAILURE;
}

void fatal(const char* Message) {
  fprintf(stderr, "%s\n", Message);
  exit(exit_status(EXIT_FAILURE));
}

void fatal(const std::string& Message) {
  fprintf(stderr, "%s\n", Message.c_str());
  exit(exit_status(EXIT_FAILURE));
}

}  // end of namespace decode

}  // end of namespace wasm
