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

// Defines predefined symbol names used in code.

#ifndef DECOMPRESSOR_SRC_SEXP_PREDEFINEDSTRINGS_DEFS_H_
#define DECOMPRESSOR_SRC_SEXP_PREDEFINEDSTRINGS_DEFS_H_

//#define X(tag, name)
#define PREDEFINED_SYMBOLS_TABLE                    \
  X(Align, "align")                                 \
  X(Binary_begin, "binary.begin")                   \
  X(Binary_bit, "binary.bit")                       \
  X(Binary_end, "binary.end")                       \
  X(Block_enter, "block.enter")                     \
  X(Block_enter_readonly, "block.enter.readonly")   \
  X(Block_enter_writeonly, "block.enter.writeonly") \
  X(Block_exit, "block.exit")                       \
  X(Block_exit_readonly, "block.exit.readonly")     \
  X(Block_exit_writeonly, "block.exit.writeonly")   \
  X(File, "file")

#endif  // DECOMPRESSOR_SRC_SEXP_PREDEFINEDSTRINGS_DEFS_H_
