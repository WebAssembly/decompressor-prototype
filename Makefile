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
else ifndef RELEASE
  RELEASE = 0
endif

CXX := clang++

SRCDIR = src

BUILDBASEDIR = build
ifeq ($(RELEASE), 0)
  BUILDDIR = $(BUILDBASEDIR)/debug
else
  BUILDDIR = $(BUILDBASEDIR)/release
endif
OBJDIR = $(BUILDDIR)/obj

LIBDIR = $(BUILDDIR)/lib
LIBPREFIX = wasm-decomp-

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

SEXP_DEFAULTS = $(SEXP_SRCDIR)/defaults.df

SEXP_OBJS=$(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))

SEXP_LIB = $(LIBDIR)/$(LIBPREFIX)sexp.a

###### Stream handlers ######

STRM_SRCDIR = $(SRCDIR)/stream
STRM_OBJDIR = $(OBJDIR)/stream
STRM_SRCS = \
	FileReader.cpp \
	FileWriter.cpp \
	ByteQueue.cpp \
	Cursor.cpp \
	StreamReader.cpp \
	StreamWriter.cpp \
	WriteUtils.cpp

STRM_OBJS=$(patsubst %.cpp, $(STRM_OBJDIR)/%.o, $(STRM_SRCS))

STRM_LIB = $(LIBDIR)/$(LIBPREFIX)strm.a

###### S-expression interpeter ######

INTERP_SRCDIR = $(SRCDIR)/interp
INTERP_OBJDIR = $(OBJDIR)/interp
INTERP_SRCS = \
	Interpreter.cpp \
	ReadStream.cpp \
	TraceSexpReader.cpp \
	TraceSexpReaderWriter.cpp \
	TraceSexpWriter.cpp \
	WriteStream.cpp

INTERP_OBJS=$(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))

INTERP_LIB = $(LIBDIR)/$(LIBPREFIX)interp.a

###### Executables ######

EXEC_DIR = $(SRCDIR)/exec
BUILD_EXECDIR = $(BUILDDIR)/bin

EXEC_SRCS = \
	decompress.cpp \
	decompsexp-wasm.cpp \
	decompwasm-sexp.cpp

EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%, $(EXEC_SRCS))

###### Test executables and locations ######

TEST_DIR = $(SRCDIR)/test
TEST_EXECDIR = $(BUILDDIR)/test

TEST_SRCS = \
	TestByteQueues.cpp \
	TestParser.cpp \
	TestRawStreams.cpp

TEST_EXECS=$(patsubst %.cpp, $(TEST_EXECDIR)/%, $(TEST_SRCS))

TEST_SRCS_DIR = test/test-sources

###### General compilation definitions ######

LIBS = $(PARSER_LIB) $(BINARY_LIB) $(INTERP_LIB) $(SEXP_LIB) \
       $(STRM_LIB) $(UTILS_LIB)
 
$(info -----------------------------------------------)
$(info Using CXX = $(CXX))
$(info Using RELEASE = $(RELEASE))
$(info -----------------------------------------------)

CCACHE := `command -v ccache`
CPP_COMPILER :=  CCACHE_CPP2=yes $(CCACHE) $(CXX)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
# Note: g++ on Travis doesn't support -std=gnu++11
CXXFLAGS := -std=c++0x -Wall -Wextra -O2 -g -pedantic -MP -MD -Werror \
	-Wno-unused-parameter -fno-omit-frame-pointer -fPIC -Isrc

ifneq ($(RELEASE), 0)
  CXXFLAGS += -DNDEBUG
endif

# Add flags to handle that Travis g++ uses -std=c++0x with missing c++11 options.
ifeq ($(CXX),g++)
  CXXFLAGS += -DOVERRIDE= -DFINAL=
else
  CXXFLAGS += -DOVERRIDE=override -DFINAL=final
endif

###### Default Rule ######

all: libs execs test-execs

.PHONY: all

build-all:
	$(MAKE) DEBUG=0 RELEASE=1 all
	$(MAKE) DEBUG=1 RELEASE=0 all
	@echo "*** Built both debug and release versions ***"

.PHONY: build-all

###### Cleaning Rules #######

clean: clean-utils-objs clean-parser clean-sexp-objs clean-strm-objs \
       clean-interp-objs clean-binary-objs clean-execs clean-libs \
       clean-test-execs

.PHONY: clean

clean-all:
	$(MAKE) DEBUG=0 RELEASE=1 clean
	$(MAKE) DEBUG=1 RELEASE=0 clean
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

gen: gen-parser gen-lexer

.PHONY: gen

gen-lexer: $(PARSER_DIR)/Lexer.cpp

.PHONY: gen-lexer

gen-parser: $(PARSER_DIR)/Parser.tab.cpp

.PHONY: gen-parser

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

$(PARSER_DIR)/Lexer.cpp: $(PARSER_DIR)/Lexer.lex
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

$(BUILD_EXECDIR):
	mkdir -p $@

$(EXECS): | $(BUILD_EXECDIR)

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(BUILD_EXECDIR)/$(dep))

$(EXECS): $(BUILD_EXECDIR)/%: $(EXEC_DIR)/%.cpp $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Compiling Test Executables #######

test-execs: $(TEST_EXECS)

.PHONY: test-execs

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

-include $(foreach dep,$(TEST_SRCS:.cpp=.d),$(TEST_EXECDIR)/$(dep))

$(TEST_EXECDIR)/TestParser: $(TEST_DIR)/TestParser.cpp $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

$(TEST_EXECDIR)/TestRawStreams: $(TEST_DIR)/TestRawStreams.cpp $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

$(TEST_EXECDIR)/TestByteQueues: $(TEST_DIR)/TestByteQueues.cpp $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Testing ######

test: all test-parser test-raw-streams test-byte-queues test-decompress \
	test-decompsexp-wasm test-decompwasm-sexp
	@echo "*** all tests passed ***"

.PHONY: test

test-all:
	$(MAKE) DEBUG=0 RELEASE=1 test
	$(MAKE)  DEBUG=1 RELEASE=0 test
	@echo "*** all tests passed on both debug and release builds ***"

.PHONY: test-all

presubmit:
	$(MAKE) clean-all
	$(MAKE) test-all CXX=g++
	$(MAKE) clean-all
	$(MAKE) test-all CXX=clang++
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
	cd test/test-sources; make test RELEASE=$(RELEASE)
	@echo "*** decompress tests passed ***"

.PHONY: test-decompress

test-decompsexp-wasm: $(BUILD_EXECDIR)/decompsexp-wasm
	$< -m < $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.wasm
	$< < $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.wasm-w
	@echo "*** sexp2wasm tests passed ***"

test-decompwasm-sexp: $(BUILD_EXECDIR)/decompwasm-sexp
	$< < test/test-sources/defaults.wasm \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	$< < test/test-sources/defaults.wasm-w \
		| diff - $(TEST_SRCS_DIR)/defaults.df
	@echo "*** wasm2sexp tests passed ***"

test-parser: $(TEST_EXECDIR)/TestParser
	$< -w -f $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.dff
	$< -w $(SEXP_DEFAULTS) | diff - $(TEST_SRCS_DIR)/defaults.df
	$< --expect-fail $(TEST_SRCS_DIR)/MismatchedParens.df 2>&1 | \
		diff - $(TEST_SRCS_DIR)/MismatchedParens.df-out
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

