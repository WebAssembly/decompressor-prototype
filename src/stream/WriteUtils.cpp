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

// Implements write utilities.

#include "stream/WriteUtils.h"

namespace wasm {

namespace decode {

namespace {

const char* ValueFormatName[] = {"decimal", "signed decimal", "hexidecimal"};

IntType IntTypeMaxPower10 = 0;  // Zero implies need computing.

IntType getIntTypeMaxPower10() {
  if (IntTypeMaxPower10)
    return IntTypeMaxPower10;
  // Compute that maximum power of 10 that can still be an IntType.
  decode::IntType MaxPower10 = 1;
  decode::IntType NextMaxPower10 = MaxPower10 * 10;
  while (NextMaxPower10 > MaxPower10) {
    MaxPower10 = NextMaxPower10;
    NextMaxPower10 *= 10;
  }
  return IntTypeMaxPower10 = MaxPower10;
}

decode::IntType divideByPower10(decode::IntType Value,
                                decode::IntType Power10) {
  if (Power10 <= 1)
    return Value;
  return Value / Power10;
}

decode::IntType moduloByPower10(decode::IntType Value,
                                decode::IntType Power10) {
  if (Power10 <= 1)
    return Value;
  return Value % Power10;
}

}  // end of anonymous namespace

const char* getName(ValueFormat Format) {
  size_t Index = size_t(Format);
  return Index < size(ValueFormatName) ? ValueFormatName[Index]
                                       : "unknown format";
}

void writeInt(WriteIntBufferType Buffer, IntType Value, ValueFormat Format) {
  size_t BufferSize = 0;
  switch (Format) {
    case ValueFormat::SignedDecimal: {
      decode::SignedIntType SignedValue = decode::SignedIntType(Value);
      if (SignedValue < 0) {
        Buffer[BufferSize++] = '-';
        Value = decode::IntType(-SignedValue);
      }
    }
    // Intentionally fall to next case.
    case ValueFormat::Decimal: {
      decode::IntType Power10 = getIntTypeMaxPower10();
      bool StartPrinting = false;
      while (Power10 > 0) {
        decode::IntType Digit = divideByPower10(Value, Power10);
        if (StartPrinting || Digit) {
          if (StartPrinting || Digit != 0) {
            StartPrinting = true;
            Buffer[BufferSize++] = '0' + Digit;
          }
        }
        Value = moduloByPower10(Value, Power10);
        Power10 /= 10;
      }
      if (!StartPrinting)
        Buffer[BufferSize++] = '0';
      break;
    }
    case ValueFormat::Hexidecimal: {
      constexpr decode::IntType BitsInHex = 4;
      decode::IntType Shift = sizeof(decode::IntType) * CHAR_BIT;
      bool StartPrinting = false;
      Buffer[BufferSize++] = '0';
      Buffer[BufferSize++] = 'x';
      while (Shift > 0) {
        Shift -= BitsInHex;
        decode::IntType Digit = (Value >> Shift);
        if (StartPrinting || Digit != 0) {
          StartPrinting = true;
          Buffer[BufferSize++] = getHexCharForDigit(Digit);
          Value &= (1 << Shift) - 1;
        }
      }
      if (!StartPrinting)
        Buffer[BufferSize++] = getHexCharForDigit(0);
      break;
    }
  }
  Buffer[BufferSize++] = '\0';
}

void writeInt(FILE* File, IntType Value, ValueFormat Format) {
  WriteIntBufferType Buffer;
  writeInt(Buffer, Value, Format);
  fprintf(File, "%s", Buffer);
}

}  // end of namespace decode

}  // end of namespace wasm
