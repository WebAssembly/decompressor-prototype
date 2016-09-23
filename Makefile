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

###### Utilities ######

UTILS_DIR = $(SRCDIR)/utils
UTILS_OBJDIR = $(OBJDIR)/utils
UTILS_OBJDIR_BOOT = $(OBJDIR_BOOT)/utils
UTILS_SRCS = \
	Allocator.cpp \
	Defs.cpp \
	Trace.cpp

UTILS_OBJS=$(patsubst %.cpp, $(UTILS_OBJDIR)/%.o, $(UTILS_SRCS))
UTILS_OBJS_BOOT=$(patsubst %.cpp, $(UTILS_OBJDIR_BOOT)/%.o, $(UTILS_SRCS))

UTILS_LIB = $(LIBDIR)/$(LIBPREFIX)utis.a
UTILS_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)utis.a

###### Binary generation objects and locations ######

BINARY_DIR = $(SRCDIR)/binary
BINARY_OBJDIR = $(OBJDIR)/binary
BINARY_OBJDIR_BOOT = $(OBJDIR_BOOT)/binary
BINARY_SRCS = \
	BinaryWriter.cpp \
	BinaryReader.cpp \
	SectionSymbolTable.cpp

BINARY_OBJS=$(patsubst %.cpp, $(BINARY_OBJDIR)/%.o, $(BINARY_SRCS))
BINARY_OBJS_BOOT=$(patsubst %.cpp, $(BINARY_OBJDIR_BOOT)/%.o, $(BINARY_SRCS))

BINARY_LIB = $(LIBDIR)/$(LIBPREFIX)binary.a
BINARY_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)binary.a

###### Parse objects and locations ######

PARSER_DIR = $(SRCDIR)/sexp-parser
PARSER_OBJDIR = $(OBJDIR)/sexp-parser
PARSER_OBJDIR_BOOT = $(OBJDIR_BOOT)/sexp-parser
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
PARSER_OBJS_BOOT=$(patsubst %.cpp, $(PARSER_OBJDIR_BOOT)/%.o, $(PARSER_SRCS))

PARSER_LIB = $(LIBDIR)/$(LIBPREFIX)parser.a
PARSER_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)parser.a

###### Filter s-expressions ######

SEXP_SRCDIR = $(SRCDIR)/sexp
SEXP_OBJDIR = $(OBJDIR)/sexp
SEXP_OBJDIR_BOOT = $(OBJDIR_BOOT)/sexp
SEXP_SRCS_BASE = \
	Ast.cpp \
	TextWriter.cpp \
	TraceSexp.cpp

SEXP_GENSRCS = \
	defaults.cpp

SEXP_DEFAULTS = $(SEXP_SRCDIR)/defaults.df
SEXP_DEFAULTS_CPP = $(SEXP_SRCDIR)/defaults.cpp

SEXP_SRCS = $(SEXP_SRCS_BASE) $(SEXP_GENSRCS)

SEXP_SRCS_BOOT = $(SEXP_SRCS_BASE)

SEXP_OBJS = $(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))
SEXP_OBJS_BOOT = $(patsubst %.cpp, $(SEXP_OBJDIR_BOOT)/%.o, $(SEXP_SRCS_BOOT))

SEXP_LIB = $(LIBDIR)/$(LIBPREFIX)sexp.a
SEXP_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)sexp.a

###### Stream handlers ######

STRM_SRCDIR = $(SRCDIR)/stream
STRM_OBJDIR = $(OBJDIR)/stream
STRM_OBJDIR_BOOT = $(OBJDIR_BOOT)/stream

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

STRM_OBJS = $(patsubst %.cpp, $(STRM_OBJDIR)/%.o, $(STRM_SRCS))
STRM_OBJS_BOOT = $(patsubst %.cpp, $(STRM_OBJDIR_BOOT)/%.o, $(STRM_SRCS))

STRM_LIB = $(LIBDIR)/$(LIBPREFIX)strm.a
STRM_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)strm.a

###### S-expression interpeter ######

INTERP_SRCDIR = $(SRCDIR)/interp
INTERP_OBJDIR = $(OBJDIR)/interp
INTERP_OBJDIR_BOOT = $(OBJDIR_BOOT)/interp

INTERP_SRCS = \
	ByteReadStream.cpp \
	ByteWriteStream.cpp \
	Interpreter.cpp \
	TraceSexpReader.cpp \
	TraceSexpReaderWriter.cpp \
	TraceSexpWriter.cpp

INTERP_OBJS = $(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))
INTERP_OBJS_BOOT = $(patsubst %.cpp, $(INTERP_OBJDIR_BOOT)/%.o, $(INTERP_SRCS))

INTERP_LIB = $(LIBDIR)/$(LIBPREFIX)interp.a
INTERP_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)interp.a

###### Executables ######

EXEC_DIR = $(SRCDIR)/exec
EXEC_OBJDIR = $(OBJDIR)/exec
EXEC_OBJDIR_BOOT = $(OBJDIR_BOOT)/exec
BUILD_EXECDIR = $(BUILDDIR)/bin
BUILD_EXECDIR_BOOT = $(BUILDDIR_BOOT)/bin

EXEC_SRCS_BASE = \
	decompress.cpp \
	decompwasm-sexp.cpp

EXEC_SRCS_BOOT = \
	decompsexp-wasm.cpp

EXEC_SRCS = $(EXEC_SRCS_BASE)  $(EXEC_SRCS_BOOT)

EXEC_OBJS = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS))

EXEC_OBJS_BOOT = $(patsubst %.cpp, $(EXEC_OBJDIR_BOOT)/%.o, $(EXEC_SRCS_BOOT))

EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%$(EXE), $(EXEC_SRCS))

EXECS_BOOT = $(patsubst %.cpp, $(BUILD_EXECDIR_BOOT)/%$(EXE), $(EXEC_SRCS_BOOT))

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

LIBS_BOOT = $(PARSER_LIB_BOOT) $(BINARY_LIB_BOOT) $(INTERP_LIB_BOOT) \
       $(SEXP_LIB_BOOT) $(STRM_LIB_BOOT) $(UTILS_LIB_BOOT)

##### Track additional important variable definitions not in Makefile.common

CCACHE := `command -v ccache`
CPP_COMPILER := CCACHE_CPP2=yes $(CCACHE) $(CXX)
CPP_COMPILER_BOOT := CCACHE_CPP2=yes $(CCACHE) $(CXX_BOOT)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
# Note: g++ on Travis doesn't support -std=gnu++11
CXXFLAGS_BASE := -Wall -Wextra -O2 -g -pedantic -MP -MD \
	    -Werror -Wno-unused-parameter -fno-omit-frame-pointer -fPIC \
	    -Isrc
CXXFLAGS := $(TARGET_CXXFLAGS) $(PLATFORM_CXXFLAGS) -DBOOTSTRAP=0 \
	    $(CXXFLAGS_BASE)

CXXFLAGS_BOOT := $(PLATFORM_CXXFLAGS_DEFAULT) -DBOOTSTRAP=1 $(CXXFLAGS_BASE)

ifneq ($(RELEASE), 0)
  CXXFLAGS += -DNDEBUG
endif

ifdef MAKE_PAGE_SIZE
  CXXFLAGS += -DWASM_DECODE_PAGE_SIZE=$(PAGE_SIZE)
endif

###### Default Rule ######

all: libs execs test-execs

.PHONY: all

###### Build boot executables ######

boot: $(EXECS_BOOT)

.PHONY: boot

###### Cleaning Rules #######

## TODO(karlschimpf): Fix clean to handle sources!

clean: clean-gen
	rm -rf $(BUILDDIR) $(BUILDDIR_BOOT)

.PHONY: clean

clean-all: clean-gen
	rm -rf $(BUILDBASEDIR)

.PHONY: clean-all

clean-gen:
	rm -rf $(PARSER_GENSRCS) $(SEXP_GENSRCS)

###### Source Generation Rules #######

gen: gen-parser gen-lexer boot

.PHONY: gen

gen-lexer: $(PARSER_DIR)/Lexer.cpp

.PHONY: gen-lexer

gen-parser: $(PARSER_DIR)/Parser.tab.cpp

.PHONY: gen-parser

$(SEXP_DEFAULTS_CPP): $(BUILD_EXECDIR_BOOT)/decompsexp-wasm $(SEXP_DEFAULTS)
	$< -i $(SEXP_DEFAULTS) -o $@

###### Compiliing binary generation Sources ######

binary-objs: $(BINARY_OBJS) $(BINARY_OBJS_BOOT)

.PHONY: binary-objs

$(BINARY_OBJS): | $(BINARY_OBJDIR)

$(BINARY_OBJDIR):
	mkdir -p $@

$(BINARY_OBJS_BOOT): | $(BINARY_OBJDIR_BOOT)

$(BINARY_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR)/$(dep))

$(BINARY_OBJS): $(BINARY_OBJDIR)/%.o: $(BINARY_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR_BOOT)/$(dep))

$(BINARY_OBJS_BOOT): $(BINARY_OBJDIR_BOOT)/%.o: $(BINARY_DIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(BINARY_LIB): $(BINARY_OBJS)
	ar -rs $@ $(BINARY_OBJS)

$(BINARY_LIB_BOOT): $(BINARY_OBJS_BOOT)
	ar -rs $@ $(BINARY_OBJS_BOOT)

###### Compiliing top-level Sources ######

utils-objs: $(UTILS_OBJS) $(UTILS_OBJS_BOOT)

.PHONY: utils-objs

$(UTILS_OBJS): | $(UTILS_OBJDIR)

$(UTILS_OBJDIR):
	mkdir -p $@

$(UTILS_OBJS_BOOT): | $(UTILS_OBJDIR_BOOT)

$(UTILS_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(UTILS_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR_BOOT)/$(dep))

$(UTILS_OBJS_BOOT): $(OBJDIR_BOOT)/%.o: $(SRCDIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(UTILS_LIB): $(UTILS_OBJS)
	ar -rs $@ $(UTILS_OBJS)

$(UTILS_LIB_BOOT): $(UTILS_OBJS_BOOT)
	ar -rs $@ $(UTILS_OBJS_BOOT)

###### Compiling s-expression interpeter sources ######

interp-objs: $(INTERP_OBJS) $(INTERP_OBJS_BOOT)

.PHONY: interp-objs

$(INTERP_OBJS): | $(INTERP_OBJDIR)

$(INTERP_OBJDIR):
	mkdir -p $@

$(INTERP_OBJS_BOOT): | $(INTERP_OBJDIR_BOOT)

$(INTERP_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR)/$(dep))

$(INTERP_OBJS): $(INTERP_OBJDIR)/%.o: $(INTERP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR_BOOT)/$(dep))

$(INTERP_OBJS_BOOT): $(INTERP_OBJDIR_BOOT)/%.o: $(INTERP_SRCDIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(INTERP_LIB): $(INTERP_OBJS)
	ar -rs $@ $(INTERP_OBJS)

$(INTERP_LIB_BOOT): $(INTERP_OBJS_BOOT)
	ar -rs $@ $(INTERP_OBJS_BOOT)

###### Compiliing Sexp Sources ######

sexp-objs: $(SEXP_OBJS) $(SEXP_OBJS_BOOT)

.PHONY: sexp-objs

$(SEXP_OBJS): | $(SEXP_OBJDIR)

$(SEXP_OBJDIR):
	mkdir -p $@

$(SEXP_OBJS_BOOT): | $(SEXP_OBJDIR_BOOT)

$(SEXP_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR_BOOT)/$(dep))

$(SEXP_OBJS_BOOT): $(SEXP_OBJDIR_BOOT)/%.o: $(SEXP_SRCDIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(SEXP_LIB): $(SEXP_OBJS)
	ar -rs $@ $(SEXP_OBJS)

$(SEXP_LIB_BOOT): $(SEXP_OBJS_BOOT)
	ar -rs $@ $(SEXP_OBJS_BOOT)

###### Compiling stream sources ######

strm-objs: $(STRM_OBJS) $(STRM_OBJS_BOOT)

.PHONY: strm-objs

$(STRM_OBJS): | $(STRM_OBJDIR)

$(STRM_OBJDIR):
	mkdir -p $@

$(STRM_OBJS_BOOT): | $(STRM_OBJDIR_BOOT)

$(STRM_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR)/$(dep))

$(STRM_OBJS): $(STRM_OBJDIR)/%.o: $(STRM_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR_BOOT)/$(dep))

$(STRM_OBJS_BOOT): $(STRM_OBJDIR_BOOT)/%.o: $(STRM_SRCDIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(STRM_LIB): $(STRM_OBJS)
	ar -rs $@ $(STRM_OBJS)

$(STRM_LIB_BOOT): $(STRM_OBJS_BOOT)
	ar -rs $@ $(STRM_OBJS_BOOT)

###### Compiling Filter Parser #######

parser-objs: $(PARSER_OBJS) $(PARSER_OBJS_BOOT)

.PHONY: parser-objs

$(PARSER_DIR)/Lexer.cpp: $(PARSER_DIR)/Lexer.lex $(PARSER_DIR)/Parser.tab.cpp
	cd $(PARSER_DIR); lex -o Lexer.cpp Lexer.lex

$(PARSER_DIR)/Parser.tab.cpp: $(PARSER_DIR)/Parser.ypp
	cd $(PARSER_DIR); bison -d -r all Parser.ypp

$(PARSER_OBJS): | $(PARSER_OBJDIR)

$(PARSER_OBJDIR):
	mkdir -p $@

$(PARSER_OBJS_BOOT): | $(PARSER_OBJDIR_BOOT)

$(PARSER_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp \
	        $(PARSER_DIR)/Lexer.cpp $(PARSER_DIR)/Parser.tab.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_OBJS_BOOT): $(PARSER_OBJDIR_BOOT)/%.o: $(PARSER_DIR)/%.cpp \
	        $(PARSER_DIR)/Lexer.cpp $(PARSER_DIR)/Parser.tab.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(PARSER_LIB): $(PARSER_OBJS)
	ar -rs $@ $(PARSER_OBJS)

$(PARSER_LIB_BOOT): $(PARSER_OBJS_BOOT)
	ar -rs $@ $(PARSER_OBJS_BOOT)

###### Building libraries ######

libs: $(LIBS) $(LIBS_BOOT)

.PHONY: libs

$(LIBDIR):
	mkdir -p $@

$(LIBS): | $(LIBDIR)

$(LIBDIR_BOOT):
	mkdir -p $@

$(LIBS_BOOT): | $(LIBDIR_BOOT)

###### Compiling executables ######

execs: $(LIBS_BOOT) $(EXECS_BOOT) $(LIBS) $(EXECS)

.PHONY: execs

$(EXEC_OBJDIR):
	mkdir -p $@

$(EXEC_OBJS): | $(EXEC_OBJDIR)

$(EXEC_OBJDIR_BOOT):
	mkdir -p $@

$(EXEC_OBJS_BOOT): | $(EXEC_OBJDIR_BOOT)

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS): $(EXEC_OBJDIR)/%.o: $(EXEC_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR_BOOT)/$(dep))

$(EXEC_OBJS_BOOT): $(EXEC_OBJDIR_BOOT)/%.o: $(EXEC_DIR)/%.cpp
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(BUILD_EXECDIR):
	mkdir -p $@

$(BUILD_EXECDIR_BOOT):
	mkdir -p $@

$(EXECS): | $(BUILD_EXECDIR)

$(EXECS): $(BUILD_EXECDIR)/%$(EXE): $(EXEC_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

$(EXECS_BOOT): | $(BUILD_EXECDIR_BOOT)

$(EXECS_BOOT): $(BUILD_EXECDIR_BOOT)/%$(EXE): $(EXEC_OBJDIR_BOOT)/%.o $(LIBS_BOOT)
	$(CPP_COMPILER_BOOT) $(CXXFLAGS_BOOT) $< $(LIBS_BOOT) -o $@

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

test: test-parser test-raw-streams test-byte-queues \
	test-decompsexp-wasm test-decompwasm-sexp test-param-passing \
        test-decompress
	@echo "*** all tests passed ***"

.PHONY: test

test-all:
	@echo "*** testing release version ***"
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=0 RELEASE=1 BOOTSTRAP=0 test
	@echo "*** testing debug version ***"
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
