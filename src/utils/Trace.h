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

// Defines a method trace utility.

#ifndef DECOMPRESSOR_SRC_UTILS_TRACE_H
#define DECOMPRESSOR_SRC_UTILS_TRACE_H

#include "utils/Defs.h"

#include <vector>

namespace wasm {

namespace utils {

class TraceClass {
  TraceClass(const TraceClass&) = delete;
  TraceClass& operator=(const TraceClass&) = delete;

 public:
  class Method {
    Method() = delete;
    Method(const Method&) = delete;
    Method& operator=(const Method&) = delete;

   public:
    Method(const char* Name, TraceClass& Cls) : Cls(Cls) {
      if (Cls.TraceProgress) {
        Cls.enter(Name);
      }
    }
    ~Method() {
      if (Cls.TraceProgress) {
        Cls.exit();
      }
    }

   private:
    TraceClass& Cls;
  };

  TraceClass();
  explicit TraceClass(const char* Label);
  explicit TraceClass(FILE* File);
  TraceClass(const char* Label, FILE* File);
  virtual ~TraceClass();
  virtual void traceContext() const;
  void indent();
  void traceMessage(const char* Message) {
    if (TraceProgress)
      traceMessageInternal(Message);
  }
  void traceMessage(const std::string& Message) {
    if (TraceProgress)
      traceMessageInternal(Message);
  }
  void traceBool(const char* Name, bool Value) {
    if (TraceProgress)
      traceBoolInternal(Name, Value);
  }
  void traceChar(const char* Name, char Ch) {
    if (TraceProgress)
      traceCharInternal(Name, Ch);
  }
  void traceSignedChar(const char* Name, signed char Ch) {
    if (TraceProgress)
      traceCharInternal(Name, char(Ch));
  }
  void traceUnsignedChar(const char* Name, unsigned char Ch) {
    if (TraceProgress)
      traceCharInternal(Name, char(Ch));
  }
  void traceString(const char* Name, std::string& Value) {
    if (TraceProgress)
      traceStringInternal(Name, Value);
  }
  void traceShort(const char* Name, short Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUnsignedShort(const char* Name, unsigned short Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceInt(const char* Name, int Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUnsignedInt(const char* Name, unsigned int Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceLong(const char* Name, long Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUnsignedLong(const char* Name, unsigned long Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceInt8_t(const char* Name, int8_t Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUint8_t(const char* Name, uint8_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceHexUint8_t(const char* Name, uint8_t Value) {
    if (TraceProgress)
      traceHexInternal(Name, Value);
  }
  void traceInt16_t(const char* Name, int16_t Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUint16_t(const char* Name, uint16_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceInt32_t(const char* Name, int32_t Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUint32_t(const char* Name, uint32_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceHexUint32_t(const char* Name, uint32_t Value) {
    if (TraceProgress)
      traceHexInternal(Name, Value);
  }
  void traceInt64_t(const char* Name, int64_t Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceIntmax_t(const char* Name, intmax_t Value) {
    if (TraceProgress)
      traceIntInternal(Name, Value);
  }
  void traceUint64_t(const char* Name, uint64_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceUintmax_t(const char* Name, uintmax_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceIntType(const char* Name, decode::IntType Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceSize_t(const char* Name, size_t Value) {
    if (TraceProgress)
      traceUintInternal(Name, Value);
  }
  void traceHexSize_t(const char* Name, size_t Value) {
    if (TraceProgress)
      traceHexInternal(Name, Value);
  }
  void tracePointer(const char* Name, void* Ptr) {
    if (TraceProgress)
      tracePointerInternal(Name, Ptr);
  }
  bool getTraceProgress() const { return TraceProgress; }
  void setTraceProgress(bool NewValue) { TraceProgress = NewValue; }

  FILE* getFile() const { return File; }

 protected:
  const char* Label;
  FILE* File;
  int IndentLevel;
  bool TraceProgress;
  std::vector<const char*> CallStack;

  void init() {
    Label = nullptr;
    File = stderr;
    IndentLevel = 0;
    TraceProgress = false;
  }

  void enter(const char* Name);
  void exit();
  void traceMessageInternal(const std::string& Message);
  void traceBoolInternal(const char* Name, bool Value);
  void traceCharInternal(const char* Name, char Ch);
  void traceStringInternal(const char* Name, std::string& Value);
  void traceIntInternal(const char* Name, intmax_t Value);
  void traceUintInternal(const char* Name, uintmax_t Value);
  void traceHexInternal(const char* Name, uintmax_t Value);
  void tracePointerInternal(const char* Name, void* Value);
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_TRACE_H
