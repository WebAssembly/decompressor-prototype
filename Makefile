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

# Note: If using -jN, be sure to run "make gen" first.

CPP_COMPILER ?= clang++

SRCDIR = src
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

LIBDIR = $(BUILDDIR)/lib
LIBPREFIX = wasm-decomp-

###### Decoder objects and locations ######

TL_SRCS = \
	Allocator.cpp \
	Defs.cpp

TL_OBJS=$(patsubst %.cpp, $(OBJDIR)/%.o, $(TL_SRCS))

TL_LIB = $(LIBDIR)/$(LIBPREFIX)tl.a

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
	TextWriter.cpp

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
	ReadStream.cpp \
	State.cpp \
	WriteStream.cpp

INTERP_OBJS=$(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))

INTERP_LIB = $(LIBDIR)/$(LIBPREFIX)interp.a

###### Executables ######

EXEC_DIR = exec
BUILD_EXECDIR = $(BUILDDIR)/bin

EXEC_SRCS = \
	decompress.cpp \
	decompsexp-wasm.cpp

EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%, $(EXEC_SRCS))

###### Test executables and locations ######

TEST_DIR = test
TEST_EXECDIR = $(BUILDDIR)/test

TEST_SRCS = \
	TestByteQueues.cpp \
	TestParser.cpp \
	TestRawStreams.cpp

TEST_EXECS=$(patsubst %.cpp, $(TEST_EXECDIR)/%, $(TEST_SRCS))

TEST_SRCS_DIR = $(TEST_DIR)/test-sources

###### General compilation definitions ######

LIBS = $(TL_LIB) $(PARSER_LIB) $(SEXP_LIB) $(INTERP_LIB) $(STRM_LIB)

$(info -----------------------------------------------)
$(info Using CPP_COMPILER = $(CPP_COMPILER))
$(info -----------------------------------------------)

CCACHE := `command -v ccache`
CXX :=  CCACHE_CPP2=yes $(CCACHE) $(CPP_COMPILER)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
CXXFLAGS := -std=gnu++11 -Wall -Wextra -O2 -g -pedantic -MP -MD -Werror \
	-Wno-unused-parameter -fno-omit-frame-pointer -fPIC -Isrc

###### Default Rule ######

all: tl-objs parser-objs sexp-objs strm-objs interp-objs libs execs \
     test-execs

.PHONY: all

###### Cleaning Rules #######

clean: clean-tl-objs clean-parser clean-sexp-objs clean-strm-objs \
       clean-interp-objs clean-execs clean-libs clean-test-execs

.PHONY: clean

clean-all: clean
	rm -rf $(BUILDDIR)

.PHONY: clean-all

clean-libs:
	rm -f $(LIBS)

.PHONY: clean-libs

clean-tl-objs:
	rm -f $(TL_OBJS)

.PHONY: clean-tl-objs

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

.PHONY: ge-parser

###### Compiliing Sources ######

tl-objs: $(TL_OBJS)

.PHONY: tl-objs

$(TL_OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(TL_SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(TL_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TL_LIB): $(TL_OBJS)
	ar -rs $@ $(TL_OBJS)

###### Compiling s-expression interpeter sources ######

interp-objs: $(INTERP_OBJS)

.PHONY: interp-objs

$(INTERP_OBJS): | $(INTERP_OBJDIR)

$(INTERP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR)/$(dep))

$(INTERP_OBJS): $(INTERP_OBJDIR)/%.o: $(INTERP_SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

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
	$(CXX) -c $(CXXFLAGS) $< -o $@

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
	$(CXX) -c $(CXXFLAGS) $< -o $@

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
	$(CXX) -c $(CXXFLAGS) $< -o $@

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
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

#$(BUILD_EXECDIR)/decompress: $(EXEC_DIR)/decompress.cpp $(LIBS)
#	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

###### Compiling Test Executables #######

test-execs: $(TEST_EXECS)

.PHONY: test-execs

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

-include $(foreach dep,$(TEST_SRCS:.cpp=.d),$(TEST_EXECDIR)/$(dep))

$(TEST_EXECDIR)/TestParser: $(TEST_DIR)/TestParser.cpp $(LIBS)
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

$(TEST_EXECDIR)/TestRawStreams: $(TEST_DIR)/TestRawStreams.cpp $(LIBS)
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

$(TEST_EXECDIR)/TestByteQueues: $(TEST_DIR)/TestByteQueues.cpp $(LIBS)
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

###### Testing ######

test: all test-parser test-raw-streams test-byte-queues test-decompress
	@echo "*** all tests passed ***"

.PHONY: test

test-decompress: $(BUILD_EXECDIR)/decompress
	$< -d defaults.df -i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d defaults.df -m -i $(TEST_SRCS_DIR)/toy.wasm -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm
	$< -d defaults.df -i $(TEST_SRCS_DIR)/toy.wasm-w -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm-w
	$< -d defaults.df -m -i $(TEST_SRCS_DIR)/toy.wasm-w -o - \
		| diff - $(TEST_SRCS_DIR)/toy.wasm
	@echo "*** decompress tests passed ***"

.PHONY: test-decompress

test-parser: $(TEST_EXECDIR)/TestParser
	$< -w defaults.df | diff - $(TEST_SRCS_DIR)/defaults.df-w
	$< --expect-fail $(TEST_SRCS_DIR)/MismatchedParens.df 2>&1 | \
		diff - $(TEST_SRCS_DIR)/MismatchedParens.df-out
	@echo "*** parser tests passed ***"

.PHONY: test-parser

test-raw-streams: $(TEST_EXECDIR)/TestRawStreams
	$< -i defaults.df | diff - defaults.df
	$< -i defaults.df -s | diff - defaults.df
	@echo "*** test raw streams passed ***"

.PHONY: test-raw-streams

test-byte-queues: $(TEST_EXECDIR)/TestByteQueues
	$< -i defaults.df | diff - defaults.df
	$< -c 2 -i defaults.df | diff - defaults.df
	$< -c 3 -i defaults.df | diff - defaults.df
	$< -c 4 -i defaults.df | diff - defaults.df
	$< -c 5 -i defaults.df | diff - defaults.df
	$< -c 6 -i defaults.df | diff - defaults.df
	$< -c 7 -i defaults.df | diff - defaults.df
	$< -c 13 -i defaults.df | diff - defaults.df
	$< -c 119 -i defaults.df | diff - defaults.df
	$< -c 2323 -i defaults.df | diff - defaults.df
	$< -c 3231 -i defaults.df | diff - defaults.df
	@echo "*** test byte queues passed ***"

.PHONY: test-byte-queues

