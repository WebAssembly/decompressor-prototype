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

#define TRACE_METHOD_USING(Name, Trace)
#define TRACE_METHOD(Name)
#define TRACE_USING(trace, type, name, value)
#define TRACE(type, name, value)
#define TRACE_PREFIX_USING(trace, Message)
#define TRACE_PREFIX(Message)
#define TRACE_MESSAGE_USING(trace, Message)
#define TRACE_MESSAGE(Message)
#define TRACE_ENTER_USING(name, trace)
#define TRACE_ENTER(name)
#define TRACE_EXIT_USING(trace)
#define TRACE_EXIT()
#define TRACE_EXIT_USING_OVERRIDE(trace, name)
#define TRACE_EXIT_OVERRIDE(name)
#define TRACE_BLOCK_USING(trace, code)
#define TRACE_BLOCK(code)

#else

#define TRACE_METHOD_USING(name, trace) \
  wasm::utils::TraceClass::Method tracEmethoD((name), (trace))
#define TRACE_METHOD(name) TRACE_METHOD_USING(name, getTrace())
#define TRACE_USING(trace, type, name, value) \
  do {                                        \
    auto& tracE = (trace);                    \
    if ((tracE).getTraceProgress())           \
      (tracE).trace_##type((name), (value));  \
  } while (false)
#define TRACE(type, name, value) TRACE_USING(getTrace(), type, name, value)
#define TRACE_PREFIX_USING(trace, Message) \
  do {                                     \
    auto& tracE = (trace);                 \
    if (tracE.getTraceProgress())          \
      tracE.trace_prefix(Message);         \
  } while (false)
#define TRACE_PREFIX(Message) TRACE_PREFIX_USING(getTrace(), Message)
#define TRACE_MESSAGE_USING(trace, Message) \
  do {                                      \
    auto& tracE = (trace);                  \
    if (tracE.getTraceProgress())           \
      tracE.trace_message(Message);         \
  } while (false)
#define TRACE_MESSAGE(Message) TRACE_MESSAGE_USING(getTrace(), Message)
#define TRACE_ENTER_USING(name, trace) \
  do {                                 \
    auto& tracE = (trace);             \
    if (tracE.getTraceProgress())      \
      (trace).enter((name));           \
  } while (false)
#define TRACE_ENTER(name) TRACE_ENTER_USING((name), getTrace())
#define TRACE_EXIT_USING_OVERRIDE(trace, name) \
  do {                                         \
    auto& tracE = (trace);                     \
    if (tracE.getTraceProgress())              \
      (trace).exit(name);                      \
  } while (false)
#define TRACE_EXIT_USING(trace) TRACE_EXIT_USING_OVERRIDE((trace), nullptr)
#define TRACE_EXIT() TRACE_EXIT_USING(getTrace())
#define TRACE_EXIT_OVERRIDE(name) TRACE_EXIT_USING_OVERRIDE(getTrace(), name)
#define TRACE_BLOCK_USING(trace, code) \
  do {                                 \
    auto& tracE = (trace);             \
    if (tracE.getTraceProgress()) {    \
      code                             \
    }                                  \
  } while (false)
#define TRACE_BLOCK(code) TRACE_BLOCK_USING(getTrace(), code)
#endif

#include "utils/Defs.h"

#include <memory>
#include <vector>

namespace wasm {

namespace filt {
class Node;
}  // end of namespace filt

namespace utils {

// Models calling contexts to be associated with each trace line.
class TraceContext : public std::enable_shared_from_this<TraceContext> {
  TraceContext(const TraceContext&) = delete;
  TraceContext& operator=(const TraceContext&) = delete;

 public:
  virtual ~TraceContext();
  virtual void describe(FILE* File) = 0;

 protected:
  TraceContext();
};

typedef std::shared_ptr<TraceContext> TraceContextPtr;

class TraceClass : public std::enable_shared_from_this<TraceClass> {
  TraceClass(const TraceClass&) = delete;
  TraceClass& operator=(const TraceClass&) = delete;

 public:
  typedef std::shared_ptr<TraceClass> Ptr;
  // Models called methods in traces.
  class Method {
    Method() = delete;
    Method(const Method&) = delete;
    Method& operator=(const Method&) = delete;

   public:
    Method(charstring Name, TraceClass& Cls) : Cls(Cls) {
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
  explicit TraceClass(charstring Label);
  explicit TraceClass(FILE* File);
  TraceClass(charstring Label, FILE* File);
  virtual ~TraceClass();
  void enter(charstring Name);
  void exit(charstring Name = nullptr);
  virtual void traceContext() const;
  void addContext(TraceContextPtr Ctx);
  void clearContexts();

  // Prints trace prefix only.
  FILE* indent();
  FILE* indentNewline();
  void trace_value_label(charstring Label);
  FILE* trace_prefix(charstring Message) {
    return tracePrefixInternal(Message);
  }
  FILE* trace_prefix(std::string Message) {
    return tracePrefixInternal(Message);
  }
  void trace_message(charstring Message) { traceMessageInternal(Message); }
  void trace_message(std::string Message) { traceMessageInternal(Message); }
  void trace_bool(charstring Name, bool Value) {
    traceBoolInternal(Name, Value);
  }
  void trace_char(charstring Name, char Ch) { traceCharInternal(Name, Ch); }
  void trace_signed_char(charstring Name, signed char Ch) {
    traceCharInternal(Name, char(Ch));
  }
  void trace_unsigned_char(charstring Name, unsigned char Ch) {
    traceCharInternal(Name, char(Ch));
  }
  void trace_string(charstring Name, const std::string&& Value) {
    traceStringInternal(Name, Value.c_str());
  }
  void trace_string(charstring Name, const std::string& Value) {
    traceStringInternal(Name, Value.c_str());
  }
  void trace_charstring(charstring Name, charstring Value) {
    traceStringInternal(Name, Value);
  }
  void trace_short(charstring Name, short Value) {
    traceIntInternal(Name, Value);
  }
  void trace_unsigned_short(charstring Name, unsigned short Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int(charstring Name, int Value) { traceIntInternal(Name, Value); }
  void trace_unsigned_int(charstring Name, unsigned int Value) {
    traceUintInternal(Name, Value);
  }
  void trace_long(charstring Name, long Value) {
    traceIntInternal(Name, Value);
  }
  void trace_unsigned_long(charstring Name, unsigned long Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int8_t(charstring Name, int8_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint8_t(charstring Name, uint8_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_uint8_t(charstring Name, uint8_t Value) {
    traceHexInternal(Name, Value);
  }
  void trace_int16_t(charstring Name, int16_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint16_t(charstring Name, uint16_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_int32_t(charstring Name, int32_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_hex_int32_t(charstring Name, int32_t Value) {
    traceHexInternal(Name, Value);
  }
  void trace_uint32_t(charstring Name, uint32_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_uint32_t(charstring Name, uint32_t Value) {
    traceHexInternal(Name, Value);
  }
  void trace_int64_t(charstring Name, int64_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_intmax_t(charstring Name, intmax_t Value) {
    traceIntInternal(Name, Value);
  }
  void trace_uint64_t(charstring Name, uint64_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_uintmax_t(charstring Name, uintmax_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_IntType(charstring Name, decode::IntType Value) {
    traceIntTypeInternal(Name, Value);
  }
  void trace_hex_IntType(charstring Name, decode::IntType Value) {
    traceHexIntTypeInternal(Name, Value);
  }
  void trace_size_t(charstring Name, size_t Value) {
    traceUintInternal(Name, Value);
  }
  void trace_hex_size_t(charstring Name, size_t Value) {
    traceHexInternal(Name, Value);
  }
  template <typename T>
  void trace_void_ptr(charstring Name, T* Ptr) {
    tracePointerInternal(Name, (void*)Ptr);
  }
  void trace_node_ptr(charstring Name, const filt::Node* Nd);
  bool getTraceProgress() const { return wasm::isDebug() && TraceProgress; }
  void setTraceProgress(bool NewValue) { TraceProgress = NewValue; }

  FILE* getFile() const { return File; }

 protected:
  charstring Label;
  FILE* File;
  int IndentLevel;
  bool TraceProgress;
  std::vector<charstring> CallStack;
  std::vector<TraceContextPtr> ContextList;

  FILE* tracePrefixInternal(const std::string& Message);
  void traceMessageInternal(const std::string& Message);
  void traceBoolInternal(charstring Name, bool Value);
  void traceCharInternal(charstring Name, char Ch);
  void traceStringInternal(charstring Name, charstring Value);
  void traceIntInternal(charstring Name, intmax_t Value);
  void traceUintInternal(charstring Name, uintmax_t Value);
  void traceIntTypeInternal(charstring Name, decode::IntType Value);
  void traceHexInternal(charstring Name, uintmax_t Value);
  void tracePointerInternal(charstring Name, void* Value);
  void traceHexIntTypeInternal(charstring Name, decode::IntType Value);

 private:
  inline void init();
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_TRACE_H
