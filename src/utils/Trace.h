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

#ifdef NDEBUG
#define TRACE_METHOD(Name, Trace)
#define TRACE_USING(trace, type, name, value)
#define TRACE(type, name, value)
#define TRACE_MESSAGE_USING(trace, Message)
#define TRACE_MESSAGE(Message)
#define TRACE_ENTER_USING(name, trace)
#define TRACE_ENTER(name)
#define TRACE_EXIT_USING(trace)
#define TRACE_EXIT()
#define TRACE_EXIT_USING_OVERRIDE(trace, name)
#define TRACE_EXIT_OVERRIDE(name)
#else
#define TRACE_METHOD(Name, Trace) \
  TraceClass::Method tracEmethoD((Name), (Trace))
#define TRACE_USING(trace, type, name, value)                                  \
  do {                                                                         \
    auto& tracE = (trace);                                                     \
    if ((tracE).getTraceProgress())                                            \
      (tracE).trace_##type((name), (value));                                   \
  } while (false)
#define TRACE(type, name, value) TRACE_USING(getTrace(), type, name, value)
#define TRACE_MESSAGE_USING(trace, Message)                                    \
  do {                                                                         \
    auto &tracE = (trace);                                                     \
    if (tracE.getTraceProgress())                                              \
      tracE.trace_message(Message);                                            \
  } while (false)
#define TRACE_MESSAGE(Message) TRACE_MESSAGE_USING(getTrace(), Message)
#define TRACE_ENTER_USING(name, trace)                                         \
  do {                                                                         \
    auto &tracE = (trace);                                                     \
    if (tracE.getTraceProgress())                                              \
      (trace).enter((name));                                                   \
  } while(false)
#define TRACE_ENTER(name) TRACE_ENTER_USING((name), getTrace())
#define TRACE_EXIT_USING_OVERRIDE(trace, name)                                 \
  do {                                                                         \
    auto &tracE = (trace);                                                     \
    if (tracE.getTraceProgress())                                              \
      (trace).exit(name);                                                      \
  } while (false)
#define TRACE_EXIT_USING(trace)                                                \
  TRACE_EXIT_USING_OVERRIDE((trace), nullptr)
#define TRACE_EXIT() TRACE_EXIT_USING(getTrace())
#define TRACE_EXIT_OVERRIDE(name)                                              \
  TRACE_EXIT_USING_OVERRIDE(getTrace(), name)
#endif

#include "utils/Defs.h"

#include <memory>
#include <vector>

namespace wasm {

namespace utils {

class TraceClass : std::enable_shared_from_this<TraceClass> {
  TraceClass(const TraceClass&) = delete;
  TraceClass& operator=(const TraceClass&) = delete;

 public:
  class Method {
    Method() = delete;
    Method(const Method&) = delete;
    Method& operator=(const Method&) = delete;

   public:
    Method(const char* Name, TraceClass& Cls) : Cls(Cls) {
      if (Cls.getTraceProgress()) {
        Cls.enter(Name);
      }
    }
    ~Method() {
      if (Cls.getTraceProgress()) {
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
  void enter(const char* Name);
  void exit(const char* Name = nullptr);
  virtual void traceContext() const;
  // Prints trace prefix only.
  FILE* indent();
  void trace_message(const char* Message) {
    traceMessageInternal(Message);
  }
  void trace_message(std::string Message) {
    traceMessageInternal(Message);
  }
  void trace_bool(const char* Name, bool Value) {
    traceBoolInternal(Name, Value);
  }
  void trace_char(const char* Name, char Ch) {
    traceCharInternal(Name, Ch);
  }
  void trace_signed_char(const char* Name, signed char Ch) {
    traceCharInternal(Name, char(Ch));
  }
  void trace_unsigned_char(const char* Name, unsigned char Ch) {
    traceCharInternal(Name, char(Ch));
  }
  void trace_string(const char* Name, const std::string&& Value) {
    traceStringInternal(Name, Value.c_str());
  }
  void trace_string(const char* Name, const std::string& Value) {
    traceStringInternal(Name, Value.c_str());
  }
  void trace_string(const char* Name, const char* Value) {
    traceStringInternal(Name, Value);
  }
  void trace_short(const char* Name, short Value) {
    traceIntInternal(Name, Value);
  }
  void trace_unsigned_short(const char* Name, unsigned short Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int(const char* Name, int Value) {
    traceIntInternal(Name, Value);
  }
  void trace_unsigned_int(const char* Name, unsigned int Value) {
    traceUintInternal(Name, Value);
  }
  void trace_long(const char* Name, long Value) {
    traceIntInternal(Name, Value);
  }
  void trace_unsigned_long(const char* Name, unsigned long Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int8_t(const char* Name, int8_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint8_t(const char* Name, uint8_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_uint8_t(const char* Name, uint8_t Value) {
    traceHexInternal(Name, Value);
  }
  void trace_int16_t(const char* Name, int16_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint16_t(const char* Name, uint16_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int32_t(const char* Name, int32_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint32_t(const char* Name, uint32_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_uint32_t(const char* Name, uint32_t Value) {
    traceHexInternal(Name, Value);
  }
  void trace_int64_t(const char* Name, int64_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_intmax_t(const char* Name, intmax_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint64_t(const char* Name, uint64_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_uintmax_t(const char* Name, uintmax_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_IntType(const char* Name, decode::IntType Value) {
    traceUintInternal(Name, Value);
  }
  void trace_size_t(const char* Name, size_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_size_t(const char* Name, size_t Value) {
      traceHexInternal(Name, Value);
  }
  template <typename T>
  void trace_void_ptr(const char* Name, T* Ptr) {
    tracePointerInternal(Name, (void*)Ptr);
  }

  // TODO(karlschimpf): The following are being deprecated.
  void traceMessage(const std::string& Message) {
    if (getTraceProgress())
      traceMessageInternal(Message);
  }
  void traceBool(const char* Name, bool Value) {
    if (getTraceProgress())
      traceBoolInternal(Name, Value);
  }
  void traceChar(const char* Name, char Ch) {
    if (getTraceProgress())
      traceCharInternal(Name, Ch);
  }
  void traceSignedChar(const char* Name, signed char Ch) {
    if (getTraceProgress())
      traceCharInternal(Name, char(Ch));
  }
  void traceUnsignedChar(const char* Name, unsigned char Ch) {
    if (getTraceProgress())
      traceCharInternal(Name, char(Ch));
  }
  void traceString(const char* Name, std::string&& Value) {
    if (getTraceProgress())
      traceStringInternal(Name, Value.c_str());
  }
  void traceString(const char* Name, std::string& Value) {
    if (getTraceProgress())
      traceStringInternal(Name, Value.c_str());
  }
  void traceShort(const char* Name, short Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUnsignedShort(const char* Name, unsigned short Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceInt(const char* Name, int Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUnsignedInt(const char* Name, unsigned int Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceLong(const char* Name, long Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUnsignedLong(const char* Name, unsigned long Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceInt8_t(const char* Name, int8_t Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUint8_t(const char* Name, uint8_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceHexUint8_t(const char* Name, uint8_t Value) {
    if (getTraceProgress())
      traceHexInternal(Name, Value);
  }
  void traceInt16_t(const char* Name, int16_t Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUint16_t(const char* Name, uint16_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceInt32_t(const char* Name, int32_t Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUint32_t(const char* Name, uint32_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceHexUint32_t(const char* Name, uint32_t Value) {
    if (getTraceProgress())
      traceHexInternal(Name, Value);
  }
  void traceInt64_t(const char* Name, int64_t Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceIntmax_t(const char* Name, intmax_t Value) {
    if (getTraceProgress())
      traceIntInternal(Name, Value);
  }
  void traceUint64_t(const char* Name, uint64_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceUintmax_t(const char* Name, uintmax_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceIntType(const char* Name, decode::IntType Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceSize_t(const char* Name, size_t Value) {
    if (getTraceProgress())
      traceUintInternal(Name, Value);
  }
  void traceHexSize_t(const char* Name, size_t Value) {
    if (getTraceProgress())
      traceHexInternal(Name, Value);
  }
  template <typename T>
  void tracePointer(const char* Name, T* Ptr) {
    if (getTraceProgress())
      tracePointerInternal(Name, (void*)Ptr);
  }
  bool getTraceProgress() const { return wasm::isDebug() && TraceProgress; }
  void setTraceProgress(bool NewValue) { TraceProgress = NewValue; }

  FILE* getFile() const { return File; }

 protected:
  const char* Label;
  FILE* File;
  int IndentLevel;
  bool TraceProgress;
  std::vector<const char*> CallStack;
  void traceMessageInternal(const std::string& Message);
  void traceBoolInternal(const char* Name, bool Value);
  void traceCharInternal(const char* Name, char Ch);
  void traceStringInternal(const char* Name, const char* Value);
  void traceIntInternal(const char* Name, intmax_t Value);
  void traceUintInternal(const char* Name, uintmax_t Value);
  void traceHexInternal(const char* Name, uintmax_t Value);
  void tracePointerInternal(const char* Name, void* Value);

 private:
  inline void init();
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_TRACE_H
