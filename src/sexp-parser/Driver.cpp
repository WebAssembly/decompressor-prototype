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

#include "sexp-parser/Driver.h"

using namespace wasm::filt;

bool Driver::parse (const std::string &Filename) {
  this->Filename = Filename;
  Begin();
  wasm::filt::Parser parser (*this);
  parser.set_debug_level(TraceParsing);
  int Result = parser.parse ();
  End ();
  return Result == 0;
}

void Driver::error (const wasm::filt::location& L, const std::string& M) const {
  std::cerr << L << ": " << M << std::endl;
}

void Driver::error (const std::string& M) const {
  error(Loc, M);
}
