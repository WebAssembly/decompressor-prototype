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

void writeInt(FILE* File, IntType Value, ValueFormat Format) {
  switch (Format) {
    case ValueFormat::SignedDecimal: {
      decode::SignedIntType SignedValue = decode::SignedIntType(Value);
      if (SignedValue < 0) {
        fputc('-', File);
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
            fputc('0' + Digit, File);
          }
        }
        Value = moduloByPower10(Value, Power10);
        Power10 /= 10;
      }
      if (!StartPrinting)
        fputc('0', File);
      break;
    }
    case ValueFormat::Hexidecimal: {
      constexpr decode::IntType BitsInHex = 4;
      decode::IntType Shift = sizeof(decode::IntType) * CHAR_BIT;
      bool StartPrinting = false;
      fputc('0', File);
      fputc('x', File);
      while (Shift > 0) {
        Shift -= BitsInHex;
        decode::IntType Digit = (Value >> Shift);
        if (StartPrinting || Digit != 0) {
          StartPrinting = true;
          fputc(getHexCharForDigit(Digit), File);
          Value &= (1 << Shift) - 1;
        }
      }
      if (!StartPrinting)
        fputc(getHexCharForDigit(0), File);
      break;
    }
  }
}

}  // end of namespace decode

}  // end of namespace wasm
