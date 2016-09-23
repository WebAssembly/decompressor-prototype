# Copyright 2016 WebAssembly Community Group participants
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Build debug: make
#            : make DEBUG=1
# Build release: make RELEASE=1

# Note: If using -jN, be sure to run "make gen" first.

include Makefile.common

# Defines whether we are building bootstrapped subsystem, so that we can
# generate corresponding source files.
ifndef BOOTSTRAP
   BOOTSTRAP=0
endif

ifneq ($(BOOTSTRAP), 0)
  BOOTSTRAP_SUBDIR=/boot
  RELEASE=0
  DEBUG=1
  PAGE_SIZE=0
  WASM=0
  WASM_VANILLA=0
  ifndef BOOTSTRAP_CXX
    CXX=clang++
  endif
endif

BOOTSTRAP_CXXFLAGS = -DBOOTSTRAP=$(BOOTSTRAP)

# Define if release or debug build
ifdef DEBUG
  ifdef RELEASE
    ifeq ($(DEBUG), 0)
      ifneq ($(RELEASE), 0)
        RELEASE = 1
      else
        $(error Cant specify both DEBUG and RELEASE)
      endif
    else ifneq ($(RELEASE), 0)
      $(error Cant specify both DEBUG and RELEASE)
    endif
  else ifeq ($(DEBUG), 0)
    RELEASE = 1
  else
    RELEASE = 0
  endif
else
  ifndef RELEASE
    DEBUG = 1
    RELEASE = 0
  else ifeq ($(RELEASE), 0)
    DEBUG = 1
  else
    DEBUG = 0
  endif
endif

# PAGE_SIZE (if defined) specifies that log2 size (i.e. number of bits) to use
# for page size.
ifndef PAGE_SIZE
  PAGE_SIZE=0
endif
ifeq ($(PAGE_SIZE), 0)
  USE_PAGE_SIZE = default
  PAGE_BUILD_SUFFIX=
  MAKE_PAGE_SIZE=
else
  PAGE_BUILD_SUFFIX=/p$(PAGE_SIZE)
  USE_PAGE_SIZE = $(PAGE_SIZE)
  MAKE_PAGE_SIZE = "PAGESIZE=$(PAGE_SIZE)"
endif

CXX := clang++

WASM_INSTALL=$(PWD)/travis_deps/wasm-install
WASM_BIN=$(WASM_INSTALL)/bin
WASM_CXX=$(WASM_BIN)/emscripten/em++
ifndef WASM
  WASM=
endif
ifndef WASM_VANILLA
  WASM_VANILLA=
endif
ifeq ($(WASM),"")
  ifeq ($(WASM_VANILLA), "")
    EMSCRIPTEN_CONFIG=$(WASM_INSTALL)/../wasm_emscripten_config_vanilla
    WASM_SUBDIR=/wasm-vanilla
  else
    EMSCRIPTEN_CONFIG=$(WASM_INSTALL)/../wasm_emscripten_config
    WASM_SUBDIR=/wasm
  endif
  CXX := $(WASM_CXX)
  WASM_SYSROOT=$(WASM_INSTALL)/sysroot
  TARGET_CXXFLAGS := \
    --em-config $(EMSCRIPTEN_CONFIG) \
    --target=wasm32-unknown-unknown \
    -S --sysroot=$(WASM_INSTALL) \
    -s BINARYEN=1
  TARGET_CXXFLAGS += -DSTACK_SIZE=1044480 \
    -w -Wno-implicit-function-declaration
	EXE=.js
endif

ifeq ($(CXX),clang++)
  CXX_SUBDIR =
else
  ifeq ($(CXX), $(WASM_CXX))
    CXX_SUBDIR =
  else
    CXX_SUBDIR = /$(CXX)
  endif
endif

PLATFORM := Default

# Define platform specific stuff.
ifeq ($(PLATFORM), Travis)
  # Add flags to handle that Travis g++ uses -std=c++0x with missing c++11 options.
  ifeq ($(CXX),g++)
    PLATFORM_CXXFLAGS += -std=c++0x -DOVERRIDE= -DFINAL=
  else
    PLATFORM_CXXFLAGS += -std=c++11 -DOVERRIDE=override -DFINAL=final
  endif
else ifeq ($(PLATFORM), Default)
  PLATFORM_CXXFLAGS += --std=c++11 -DOVERRIDE= -DFINAL=
else
  $(error Unrecognized platform: $(PLATFORM))
endif

SRCDIR = src

BUILDBASEDIR = build$(BOOTSTRAP_SUBDIR)$(WASM_SUBDIR)$(CXX_SUBDIR)$(PAGE_BUILD_SUFFIX)
ifeq ($(RELEASE), 0)
  BUILDDIR = $(BUILDBASEDIR)/debug
else
  BUILDDIR = $(BUILDBASEDIR)/release
endif
OBJDIR = $(BUILDDIR)/obj

LIBDIR = $(BUILDDIR)/lib
LIBPREFIX = wasm-decomp-

MAKE_RELEASE = DEBUG=$(DEBUG) RELEASE=$(RELEASE)

###### Utilities ######

UTILS_DIR = $(SRCDIR)/utils
UTILS_OBJDIR = $(OBJDIR)/utils
UTILS_SRCS = \
	Allocator.cpp \
	Defs.cpp \
	Trace.cpp

UTILS_OBJS=$(patsubst %.cpp, $(UTILS_OBJDIR)/%.o, $(UTILS_SRCS))

UTILS_LIB = $(LIBDIR)/$(LIBPREFIX)utis.a

###### Binary generation objects and locations ######

BINARY_DIR = $(SRCDIR)/binary
BINARY_OBJDIR = $(OBJDIR)/binary
BINARY_SRCS = \
	BinaryWriter.cpp \
	BinaryReader.cpp \
	SectionSymbolTable.cpp

BINARY_OBJS=$(patsubst %.cpp, $(BINARY_OBJDIR)/%.o, $(BINARY_SRCS))

BINARY_LIB = $(LIBDIR)/$(LIBPREFIX)binary.a

###### Parse objects and locations ######

PARSER_DIR = $(SRCDIR)/sexp-parser
PARSER_OBJDIR = $(OBJDIR)/sexp-parser
PARSER_GENSRCS = \
	location.hh \
	Lexer.cpp \
	Parser.output \
	Parser.tab.cpp \
	Parser.tab.hpp \
	position.hh \
	stack.hh

PARSER_SRCS = \
	Driver.cpp \
	Lexer.cpp \
	Parser.tab.cpp

PARSER_OBJS=$(patsubst %.cpp, $(PARSER_OBJDIR)/%.o, $(PARSER_SRCS))

PARSER_LIB = $(LIBDIR)/$(LIBPREFIX)parser.a

###### Filter s-expressions ######

SEXP_SRCDIR = $(SRCDIR)/sexp
SEXP_OBJDIR = $(OBJDIR)/sexp
SEXP_SRCS = \
	Ast.cpp \
	TextWriter.cpp \
	TraceSexp.cpp

ifeq ($(BOOTSTRAP), 0)
  SEXP_GENSRCS = \
	defaults.cpp
  SEXP_SRCS += $(SEXP_GENSRCS)
  SEXP_GEN_DEPS=$(patsubst %.cpp, $(SEXP_SRRCDIR)/%.cpp, $(SEXP_GENSRCS))
endif

SEXP_DEFAULTS = $(SEXP_SRCDIR)/defaults.df
SEXP_DEFAULTS_CPP = $(SEXP_SRCDIR)/defaults.cpp

SEXP_OBJS=$(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))

SEXP_LIB = $(LIBDIR)/$(LIBPREFIX)sexp.a

###### Stream handlers ######

STRM_SRCDIR = $(SRCDIR)/stream
STRM_OBJDIR = $(OBJDIR)/stream
STRM_SRCS = \
	FileReader.cpp \
	FileWriter.cpp \
	Cursor.cpp \
	Page.cpp \
	Queue.cpp \
	ReadCursor.cpp \
	ReadBackedQueue.cpp \
	StreamReader.cpp \
	StreamWriter.cpp \
	StringReader.cpp \
	StringWriter.cpp \
	WriteBackedQueue.cpp \
	WriteCursor.cpp \
	WriteUtils.cpp

STRM_OBJS=$(patsubst %.cpp, $(STRM_OBJDIR)/%.o, $(STRM_SRCS))

STRM_LIB = $(LIBDIR)/$(LIBPREFIX)strm.a

###### S-expression interpeter ######

INTERP_SRCDIR = $(SRCDIR)/interp
INTERP_OBJDIR = $(OBJDIR)/interp
INTERP_SRCS = \
	ByteReadStream.cpp \
	ByteWriteStream.cpp \
	Interpreter.cpp \
	TraceSexpReader.cpp \
	TraceSexpReaderWriter.cpp \
	TraceSexpWriter.cpp

INTERP_OBJS=$(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))

INTERP_LIB = $(LIBDIR)/$(LIBPREFIX)interp.a

###### Executables ######

EXEC_DIR = $(SRCDIR)/exec
EXEC_OBJDIR = $(OBJDIR)/exec
BUILD_EXECDIR = $(BUILDDIR)/bin

EXEC_SRCS = \
	decompress.cpp \
	decompsexp-wasm.cpp \
	decompwasm-sexp.cpp

EXEC_OBJS=$(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS))

EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%$(EXE), $(EXEC_SRCS))

BOOTSTRAP_EXEC_SRCS = \
	decompsexp-wasm.cpp

BOOTSTRAP_EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%$(EXE), \
                                    $(BOOTSTRAP_EXEC_SRCS))

###### Test executables and locations ######

TEST_DIR = $(SRCDIR)/test
TEST_OBJDIR = $(OBJDIR)/test
TEST_EXECDIR = $(BUILDDIR)/test

TEST_SRCS = \
	TestByteQueues.cpp \
	TestParser.cpp \
	TestRawStreams.cpp

TEST_OBJS=$(patsubst %.cpp, $(TEST_OBJDIR)/%.o, $(TEST_SRCS))

TEST_EXECS=$(patsubst %.cpp, $(TEST_EXECDIR)/%$(EXE), $(TEST_SRCS))

TEST_SRCS_DIR = test/test-sources

###### General compilation definitions ######

LIBS = $(PARSER_LIB) $(BINARY_LIB) $(INTERP_LIB) $(SEXP_LIB) \
       $(STRM_LIB) $(UTILS_LIB)
 
$(info Using BUILDDIR = $(BUILDDIR))
$(info -----------------------------------------------)

CCACHE := `command -v ccache`
CPP_COMPILER :=  CCACHE_CPP2=yes $(CCACHE) $(CXX)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
# Note: g++ on Travis doesn't support -std=gnu++11
CXXFLAGS := $(TARGET_CXXFLAGS) $(PLATFORM_CXXFLAGS) $(BOOTSTRAP_CXXFLAGS) \
	    -Wall -Wextra -O2 -g -pedantic -MP -MD \
	    -Werror -Wno-unused-parameter -fno-omit-frame-pointer -fPIC \
	    -Isrc

ifneq ($(RELEASE), 0)
  CXXFLAGS += -DNDEBUG
endif

ifdef MAKE_PAGE_SIZE
  CXXFLAGS += -DWASM_DECODE_PAGE_SIZE=$(PAGE_SIZE)
endif

###### Default Rule ######

all: gen
	$(MAKE) $(MAKE_PAGE_SIZE) $(MAKE_RELEASE) all-nogen

.PHONY: all

all-nogen: libs execs test-execs

.PHONY: all-nogen

build-all:
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=0 RELEASE=1 BOOTSTRAP=0 all
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=1 RELEASE=0 BOOTSTRAP=0 all
	@echo "*** Built both debug and release versions ***"

.PHONY: build-all

###### Build boot executables ######

boot:
	@echo "-> Building boot tools"
	$(MAKE) BOOTSTRAP=1 boot-tools
	@echo "<- Building boot tools"

.PHONY: boot

ifneq ($(BOOTSTRAP), 0)

boot-tools: libs $(BOOTSTRAP_EXECS) $(SEXP_DEFAULTS_CPP)

.PHONY: boot-tools

endif

###### Cleaning Rules #######

clean:
	$(MAKE) $(MAKE_PAGE_SIZE) $(MAKE_RELEASE) BOOTSTRAP=0 clean-objs
	$(MAKE) BOOTSTRAP=1 clean-objs

.PHONY: clean

clean-objs: clean-parser clean-sexp
	rm -rf $(BUILDDIR)

.PHONY: clean-objs

clean-all:
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=0 RELEASE=1 BOOTSTRAP=0 clean
	$(MAKE) BOOTSTRAP=1 clean
	rm -rf $(BUILDBASEDIR)

.PHONY: clean-all

clean-libs:
	rm -f $(LIBS)

.PHONY: clean-libs

clean-binary-objs:
	rm -f $(BINARY_OBJS)

.PHONY: clean-binary-objs

clean-utils-objs:
	rm -f $(UTILS_OBJS)

.PHONY: clean-utils-objs

clean-parser: clean-parser-objs
	cd $(PARSER_DIR); rm -f $(PARSER_GENSRCS)

.PHONY: clean-parser

clean-parser-objs:
	rm -f $(PARSER_OBJS)

.PHONY: clean-parser-objs

ifeq ($(BOOTSTRAP), 0)
clean-sexp: clean-sexp-objs
	cd $(SEXP_SRCDIR); rm -f $(SEXP_GENSRCS)
else
clean-sexp: clean-sexp-objs
endif

.PHONY: clean-sexp

clean-sexp-objs:
	rm -f $(SEXP_OBJS)

.PNONY: clean-sexp-objs

clean-strm-objs:
	rm -f $(STRM_OBJS)

.PHONY: clean-strm-objs

clean-interp-objs:
	rm -f $(INTERP_OBJS)

.PHONY: clean-interp-objs

clean-execs:
	rm -f $(EXECS)

.PHONY: clean-execs

clean-test-execs:
	rm -f $(TEST_EXECS)

.PHONY: clean-test-execs

###### Source Generation Rules #######

gen: gen-parser gen-lexer boot

.PHONY: gen

gen-lexer: $(PARSER_DIR)/Lexer.cpp

.PHONY: gen-lexer

gen-parser: $(PARSER_DIR)/Parser.tab.cpp

.PHONY: gen-parser

ifneq ($(BOOTSTRAP), 0)

$(SEXP_DEFAULTS_CPP): $(BUILD_EXECDIR)/decompsexp-wasm $(SEXP_DEFAULTS)
	$< -i $(SEXP_DEFAULTS) -o $@

.PHONY: gen-default-wasm

endif

###### Compiliing binary generation Sources ######

binary-objs: $(BINARY_OBJS)

.PHONY: binary-objs

$(BINARY_OBJS): | $(BINARY_OBJDIR)

$(BINARY_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR)/$(dep))

$(BINARY_OBJS): $(BINARY_OBJDIR)/%.o: $(BINARY_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(BINARY_LIB): $(BINARY_OBJS)
	ar -rs $@ $(BINARY_OBJS)

###### Compiliing top-level Sources ######

utils-objs: $(UTILS_OBJS)

.PHONY: utils-objs

$(UTILS_OBJS): | $(UTILS_OBJDIR)

$(UTILS_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(UTILS_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(UTILS_LIB): $(UTILS_OBJS)
	ar -rs $@ $(UTILS_OBJS)

###### Compiling s-expression interpeter sources ######

interp-objs: $(INTERP_OBJS)

.PHONY: interp-objs

$(INTERP_OBJS): | $(INTERP_OBJDIR)

$(INTERP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR)/$(dep))

$(INTERP_OBJS): $(INTERP_OBJDIR)/%.o: $(INTERP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(INTERP_LIB): $(INTERP_OBJS)
	ar -rs $@ $(INTERP_OBJS)

###### Compiliing Sexp Sources ######

sexp-objs: $(SEXP_OBJS)

.PHONY: sexp-objs

$(SEXP_OBJS): | $(SEXP_OBJDIR)

$(SEXP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(SEXP_LIB): $(SEXP_OBJS)
	ar -rs $@ $(SEXP_OBJS)

###### Compiling stream sources ######

strm-objs: $(STRM_OBJS)

.PHONY: strm-objs

$(STRM_OBJS): | $(STRM_OBJDIR)

$(STRM_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR)/$(dep))

$(STRM_OBJS): $(STRM_OBJDIR)/%.o: $(STRM_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(STRM_LIB): $(STRM_OBJS)
	ar -rs $@ $(STRM_OBJS)

###### Compiling Filter Parser #######

parser-objs: $(PARSER_OBJS)

.PHONY: parser-objs

$(PARSER_DIR)/Lexer.cpp: $(PARSER_DIR)/Lexer.lex $(PARSER_DIR)/Parser.tab.cpp
	cd $(PARSER_DIR); lex -o Lexer.cpp Lexer.lex

$(PARSER_DIR)/Parser.tab.cpp: $(PARSER_DIR)/Parser.ypp
	cd $(PARSER_DIR); bison -d -r all Parser.ypp

$(PARSER_OBJS): | $(PARSER_OBJDIR)

$(PARSER_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp \
	        $(PARSER_DIR)/Lexer.cpp $(PARSER_DIR)/Parser.tab.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(PARSER_LIB): $(PARSER_OBJS)
	ar -rs $@ $(PARSER_OBJS)

###### Building libraries ######

libs: $(LIBS)

.PHONY: libs

$(LIBDIR):
	mkdir -p $@

$(LIBS): | $(LIBDIR)

###### Compiling executables ######

execs: $(LIBS) $(EXECS)

.PHONY: execs

$(EXEC_OBJDIR):
	mkdir -p $@

$(EXEC_OBJS): | $(EXEC_OBJDIR)

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS): $(EXEC_OBJDIR)/%.o: $(EXEC_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(BUILD_EXECDIR):
	mkdir -p $@

$(EXECS): | $(BUILD_EXECDIR)

$(EXECS): $(BUILD_EXECDIR)/%$(EXE): $(EXEC_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Compiling Test Executables #######

test-execs: $(TEST_EXECS)

.PHONY: test-execs

$(TEST_OBJDIR):
	mkdir -p $@

$(TEST_OBJS): | $(TEST_OBJDIR)

-include $(foreach dep,$(TEST_SRCS:.cpp=.d),$(TEST_OBJDIR)/$(dep))

$(TEST_OBJS): $(TEST_OBJDIR)/%.o: $(TEST_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

$(TEST_EXECS): $(TEST_EXECDIR)/%$(EXE): $(TEST_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Testing ######

test: all
	$(MAKE) $(MAKE_PAGE_SIZE) $(MAKE_RELEASE) run-tests
	@echo "*** all tests passed ***"

.PHONY: test

run-tests: test-parser test-raw-streams test-byte-queues \
	test-decompsexp-wasm test-decompwasm-sexp test-param-passing \
        test-decompress
	@echo "*** all tests passed ***"

.PHONY: run-tests

test-all:
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=0 RELEASE=1 BOOTSTRAP=0 test
	$(MAKE) $(MAKE_PAGE_SIZE)  DEBUG=1 RELEASE=0 BOOTSTRAP=0 test
	@echo "*** all tests passed on both debug and release builds ***"

.PHONY: test-all

presubmit:
	$(MAKE) $(MAKE_PAGE_SIZE) clean-all
	$(MAKE) test-all CXX=clang++ PAGE_SIZE=2
	$(MAKE) $(MAKE_PAGE_SIZE) clean-all
	$(MAKE) $(MAKE_PAGE_SIZE) test-all CXX=g++
	$(MAKE) $(MAKE_PAGE_SIZE) clean-all
	$(MAKE) $(MAKE_PAGE_SIZE) test-all CXX=clang++
	$(MAKE) $(MAKE_PAGE_SIZE) clean-all
	@echo "*** Presubmit tests passed ***"

.PHONY: presubmit

test-decompress: $(BUILD_EXECDIR)/decompress
	$< -d $(SEXP_DEFAULTS) -i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d $(TEST_SRCS_DIR)/defaults.wasm \
		-i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d $(TEST_SRCS_DIR)/defaults.wasm -m \
		-i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm
	$< -d $(TEST_SRCS_DIR)/defaults.wasm \
		-i $(TEST_SRCS_DIR)/toy.wasm-w -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d $(TEST_SRCS_DIR)/defaults.wasm -m \
		-i $(TEST_SRCS_DIR)/toy.wasm-w -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm
	$< -d $(TEST_SRCS_DIR)/defaults.wasm -m \
		-i $(TEST_SRCS_DIR)/if-then-br.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/if-then-br.wasm
	cd test/test-sources; make test RELEASE=$(RELEASE) CXX=$(CXX)
	@echo "*** decompress tests passed ***"

.PHONY: test-decompress

test-param-passing: $(BUILD_EXECDIR)/decompress
	$< -d test/test-sources/defaults-param-test.df \
		-i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d test/test-sources/defaults-param-test.df \
		-i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	@echo "*** test-param-passing passed ***"

.PHONY: test-param-passing

test-decompsexp-wasm: $(BUILD_EXECDIR)/decompsexp-wasm
	$< -m -i $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.wasm
	$< -i $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.wasm-w
	@echo "*** sexp2wasm tests passed ***"

.PHONY: test-decompsexp-wasm

test-decompwasm-sexp: $(BUILD_EXECDIR)/decompwasm-sexp
	$< -i test/test-sources/defaults.wasm \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	$< -i test/test-sources/defaults.wasm-w \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	$< -r 5 -i test/test-sources/defaults.wasm \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	$< -r 5 -i test/test-sources/defaults.wasm-w \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	@echo "*** wasm2sexp tests passed ***"

.PHONY: test-decompwasm-sexp

test-parser: $(TEST_EXECDIR)/TestParser
	$< -w $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.df
	$< -w $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.df
	$< --expect-fail $(TEST_SRCS_DIR)/MismatchedParens.df 2>&1 | \
		diff - $(TEST_SRCS_DIR)/MismatchedParens.df-out
	$< -w $(TEST_SRCS_DIR)/defaults.df | \
		diff - $(TEST_SRCS_DIR)/defaults.df
	@echo "*** parser tests passed ***"

.PHONY: test-parser

test-raw-streams: $(TEST_EXECDIR)/TestRawStreams
	$< -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -i $(SEXP_DEFAULTS) -s | diff - $(SEXP_DEFAULTS)
	@echo "*** test raw streams passed ***"

.PHONY: test-raw-streams

test-byte-queues: $(TEST_EXECDIR)/TestByteQueues
	$< -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 2 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 3 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 4 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 5 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 6 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 7 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 13 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 119 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 2323 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	$< -c 3231 -i $(SEXP_DEFAULTS) | diff - $(SEXP_DEFAULTS)
	@echo "*** test byte queues passed ***"

.PHONY: test-byte-queues


###### Unit tests ######

GTEST_DIR = third_party/googletest/googletest
GTEST_INCLUDE = $(GTEST_DIR)/include
GTEST_SRC_DIR = $(GTEST_DIR)/include
GTEST_LIB = $(GTEST_DIR)/out/libgtest.a

UNITTEST_DIR = $(SRCDIR)/unit-tests
UNITTEST_EXECDIR = $(BUILDDIR)/unit-tests

UNITTEST_SRCS = \
	test-string-reader.cpp

UNITTEST_EXECS = $(patsubst %.cpp, $(UNITTEST_EXECDIR)/%$(EXE), $(UNITTEST_SRCS))

UNITTEST_TESTS = $(patsubst %.cpp, $(UNITTEST_EXECDIR)/%.test, $(UNITTEST_SRCS))

$(UNITTEST_EXECS): | $(UNITTEST_EXECDIR)

$(UNITTEST_EXECDIR):
	mkdir -p $@

-include $(foreach dep,$(UNITTEST_SRCS:.cpp=.d),$(UNITTEST_EXECDIR)/$(dep))

$(UNITTEST_EXECS): $(UNITTEST_EXECDIR)/%$(EXE): $(UNITTEST_DIR)/%.cpp $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) -I$(GTEST_INCLUDE) $< $(LIBS) \
		$(GTEST_LIB) -lpthread -o $@

$(UNITTEST_TESTS): $(UNITTEST_EXECDIR)/%.test: $(UNITTEST_EXECDIR)/%
	$<

unit-tests: $(UNITTEST_TESTS)

.PHONY: unit-tests $(UNITTEST_TESTS)

clean-unit-tests:
	rm -rf $(UNITTEST_EXECDIR)/*
