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

// Defines methods and states for the interpreter.

#ifndef DECOMPRESSOR_SRC_INTERP_INTERPRETER_DEFS_H_
#define DECOMPRESSOR_SRC_INTERP_INTERPRETER_DEFS_H_

//#define X(code, value)
#define SECTION_CODES_TABLE \
  X(Unknown, 0)             \
  X(Type, 1)                \
  X(Import, 2)              \
  X(Function, 3)            \
  X(Table, 4)               \
  X(Memory, 5)              \
  X(Global, 6)              \
  X(Export, 7)              \
  X(Start, 8)               \
  X(Elem, 9)                \
  X(Code, 10)               \
  X(Data, 11)

//#define X(tag)
#define INTERPRETER_METHODS_TABLE \
  X(CopyBlock)                    \
  X(Eval)                         \
  X(EvalBlock)                    \
  X(EvalInCallingContext)         \
  X(Finished)                     \
  X(GetAlgorithm)                 \
  X(GetFile)                      \
  X(HasFileHeader)                \
  X(Peek)                         \
  X(ReadIntBlock)                 \
  X(ReadOpcode)                   \
  X(ReadIntValues)                \
  X(Started)

//#define X(tag)
#define INTERPRETER_STATES_TABLE \
  X(Catch)                       \
  X(Enter)                       \
  X(Exit)                        \
  X(Loop)                        \
  X(Failed)                      \
  X(MinBlock)                    \
  X(Step2)                       \
  X(Step3)                       \
  X(Step4)                       \
  X(Succeeded)

//#define X(tag, flags)
// flags: bit(0)==1 => Read, bit(1)==1 => Write
#define INTERPRETER_METHOD_MODIFIERS_TABLE \
  X(ReadOnly, 0x1)                         \
  X(WriteOnly, 0x2)                        \
  X(ReadAndWrite, 0x3)

#endif  // DECOMPRESSOR_SRC_INTERP_INTERPRETER_DEFS_H_
