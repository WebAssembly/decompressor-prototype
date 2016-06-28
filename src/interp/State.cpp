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

// Implements the interpreter for filter s-expressions.

#include "interp/State.h"

namespace wasm {

using namespace alloc;
using namespace decode;
using namespace filt;

namespace interp {

namespace {

IntType getIntegerValue(Node *N) {
  if (auto *IntVal = dyn_cast<IntegerNode>(N))
    return IntVal->getValue();
  fatal("Integer value expected but not found");
  return 0;
}

} // end of anonymous namespace

State::State(ByteQueue *Input, ByteQueue *Output) :
    ReadPos(Input), WritePos(Output), Alloc(Allocator::Default) {
  Reader = Alloc->create<ByteReadStream>();
  Writer = Alloc->create<ByteWriteStream>();
  DefaultFormat = Alloc->create<Nullary<OpVaruint64NoArgs>>();
}

IntType State::eval(const Node *Nd) {
  // TODO(kschimpf): Fix for ast streams.
  // TODO(kschimpf) Handle blocks.
  switch (NodeType Type = Nd->getType()) {
    case OpAstToAst:
    case OpAstToBit:
    case OpAstToByte:
    case OpAstToInt:
    case OpBitToAst:
    case OpBitToBit:
    case OpBitToByte:
    case OpBitToInt:
    case OpByteToAst:
    case OpByteToBit:
    case OpByteToByte:
    case OpByteToInt:
    case OpSymConst:
    case OpCopy:
    case OpFilter:
    case OpIntToAst:
    case OpIntToBit:
    case OpIntToByte:
    case OpIntToInt:
    case OpSelect:
    case OpBlockBegin:
    case OpBlockEnd:
    case OpBlockOneArg:
    case OpBlockTwoArgs:
    case OpBlockThreeArgs:
    case OpCase:
    case OpSymbol:
    case OpWrite:
      // TODO(kschimpf): Fix above cases.
      fprintf(stderr, "Not implemented: %s\n", getNodeTypeName(Type));
      fatal("Unable to evaluate filter s-expression");
      return 0;
    case OpDefault:
    case OpDefine:
    case OpFile:
    case OpSection:
    case OpUndefine:
    case OpVersion:
    case OpInteger:
      fprintf(stderr, "Not evaluatable: %s\n", getNodeTypeName(Type));
      fatal("Evaluating filter s-expression not defined");
      return 0;
    case OpAppendNoArgs:
    case OpPostorder:
    case OpPreorder:
      // TODO(kschimpf) Fix to do tree operations.
      return 0;
    case OpAppendOneArg: {
      auto *Append = cast<Unary<OpAppendOneArg>>(Nd);
      return eval(Append->getKid(0));
    }
    case OpError:
      fatal("Error found during evaluation");
      return 0;
    case OpEval: {
      if (auto *Sym = dyn_cast<SymbolNode>(Nd->getKid(0))) {
        return eval(Sym->getDefineDefinition());
      }
      fatal("Can't evaluate symbol");
      return 0;
    }
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
      return read(Nd);
    case OpLit:
      // TOOD(kschimpf): Merge OpLit with OpWrite (really do same thing).
      return write(read(Nd), DefaultFormat);
    case OpLoop: {
      size_t Count = eval(Nd->getKid(0));
      size_t NumKids = Nd->getNumKids();
      for (size_t i = 0; i < Count; ++i) {
        for (size_t j = 1; j < NumKids; ++j) {
          eval(Nd->getKid(j));
        }
      }
      return 0;
    }
    case OpLoopUnbounded: {
      while (! ReadPos.atEob())
        for (auto *Kid : *Nd)
          eval(Kid);
      return 0;
    }
    case OpMap:
      return write(read(Nd->getKid(0)), Nd->getKid(1));
    case OpPeek:
      return read(Nd);
    case OpRead:
      return read(Nd->getKid(1));
    case OpSequence:
      for (auto *Kid : *Nd)
        eval(Kid);
      return 0;
    case OpUint8NoArgs:
    case OpUint8OneArg:
    case OpUint32NoArgs:
    case OpUint32OneArg:
    case OpUint64NoArgs:
    case OpUint64OneArg:
    case OpVarint32NoArgs:
    case OpVarint32OneArg:
    case OpVarint64NoArgs:
    case OpVarint64OneArg:
    case OpVaruint1NoArgs:
    case OpVaruint1OneArg:
    case OpVaruint7NoArgs:
    case OpVaruint7OneArg:
    case OpVaruint32NoArgs:
    case OpVaruint32OneArg:
    case OpVaruint64NoArgs:
    case OpVaruint64OneArg:
      return write(read(Nd), Nd);
    case OpVoid:
      return 0;
  }
}

IntType State::read(const Node *Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Read not implemented: %s\n", getNodeTypeName(Type));
      fatal("Read not implemented");
      return 0;
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
    case OpLit:
      return getIntegerValue(Nd->getKid(0));
    case OpPeek: {
      ReadCursor InitialPos(ReadPos);
      IntType Value = read(Nd->getKid(0));
      ReadPos.swap(InitialPos);
      return Value;
    }
    case OpUint8NoArgs:
      return Reader->readUint8(ReadPos);
    case OpUint8OneArg:
      return Reader->readUint8(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint32NoArgs:
      return Reader->readUint32(ReadPos);
    case OpUint32OneArg:
      return Reader->readUint32(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpUint64NoArgs:
      return Reader->readUint64(ReadPos);
    case OpUint64OneArg:
      return Reader->readUint64(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint32NoArgs:
      return Reader->readVarint32(ReadPos);
    case OpVarint32OneArg:
      return Reader->readVarint32(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVarint64NoArgs:
      return Reader->readVarint64(ReadPos);
    case OpVarint64OneArg:
      return Reader->readVarint64(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint1NoArgs:
      return Reader->readVaruint1(ReadPos);
    case OpVaruint1OneArg:
      return Reader->readVaruint1(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint7NoArgs:
      return Reader->readVaruint7(ReadPos);
    case OpVaruint7OneArg:
      return Reader->readVaruint7(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint32NoArgs:
      return Reader->readVaruint32(ReadPos);
    case OpVaruint32OneArg:
      return Reader->readVaruint32(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVaruint64NoArgs:
      return Reader->readVaruint64(ReadPos);
    case OpVaruint64OneArg:
      return Reader->readVaruint64(ReadPos, getIntegerValue(Nd->getKid(0)));
    case OpVoid:
      return 0;
  }
}

IntType State::write(IntType Value, const wasm::filt::Node *Nd) {
  switch (NodeType Type = Nd->getType()) {
    default:
      fprintf(stderr, "Read not implemented: %s\n", getNodeTypeName(Type));
      fatal("Read not implemented");
      break;
    case OpI32Const:
    case OpI64Const:
    case OpU32Const:
    case OpU64Const:
    case OpPeek:
    case OpLit:
      write(Value, DefaultFormat);
      break;
    case OpUint8NoArgs:
      Writer->writeUint8(WritePos, Value);
      break;
    case OpUint8OneArg:
      Writer->writeUint8(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpUint32NoArgs:
      Writer->writeUint32(WritePos, Value);
      break;
    case OpUint32OneArg:
      Writer->writeUint32(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpUint64NoArgs:
      Writer->writeUint64(WritePos, Value);
      break;
    case OpUint64OneArg:
      Writer->writeUint64(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVarint32NoArgs:
      Writer->writeVarint32(WritePos, Value);
      break;
    case OpVarint32OneArg:
      Writer->writeVarint32(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVarint64NoArgs:
      Writer->writeVarint64(WritePos, Value);
      break;
    case OpVarint64OneArg:
      Writer->writeVarint64(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVaruint1NoArgs:
      Writer->writeVaruint1(WritePos, Value);
      break;
    case OpVaruint1OneArg:
      Writer->writeVaruint1(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVaruint7NoArgs:
      Writer->writeVaruint7(WritePos, Value);
      break;
    case OpVaruint7OneArg:
      Writer->writeVaruint7(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVaruint32NoArgs:
      Writer->writeVaruint32(WritePos, Value);
      break;
    case OpVaruint32OneArg:
      Writer->writeVaruint32(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVaruint64NoArgs:
      Writer->writeVaruint64(WritePos, Value);
      break;
    case OpVaruint64OneArg:
      Writer->writeVaruint64(WritePos, getIntegerValue(Nd->getKid(0)), Value);
      break;
    case OpVoid:
      break;
  }
  return Value;
}

} // end of namespace interp.

} // end of namespace wasm.
