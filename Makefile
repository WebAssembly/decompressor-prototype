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

###### Decoder objects and locations ######

SRCS = \
	Defs.cpp

OBJS=$(patsubst %.cpp, $(OBJDIR)/%.o, $(SRCS))

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

###### Filter s-expressions ###

SEXP_SRCDIR = $(SRCDIR)/sexp
SEXP_OBJDIR = $(OBJDIR)/sexp
SEXP_SRCS = \
	Ast.cpp

SEXP_OBJS=$(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))

###### Test executables and locations ######

TEST_DIR = test
TEST_EXECDIR = $(BUILDDIR)/test

TEST_SRCS = \
	TestParser.cpp

TEST_EXECS=$(patsubst %.cpp, $(TEST_EXECDIR)/%, $(TEST_SRCS))

###### General compilation definitions ######

$(info -----------------------------------------------)
$(info Using CPP_COMPILER = $(CPP_COMPILER))
$(info -----------------------------------------------)

CCACHE := `command -v ccache`
CXX :=  CCACHE_CPP2=yes $(CCACHE) $(CPP_COMPILER)

CXXFLAGS := -std=gnu++11 -Wall -Wextra -O2 -g -pedantic -MP -MD -Werror -Isrc

###### Default Rule ######

all: gen objs parser-objs sexp-objs test-execs

.PHONY: all

###### Cleaning Rules #######

clean: clean-objs clean-parser clean-sexp-objs clean-test-execs

.PHONY: clean

clean-all: clean
	rm -rf $(BUILDDIR)

.PHONY: clean-all

clean-objs:
	rm -f $(OBJS)

.PHONY: clean-objs

clean-parser: clean-parser-objs
	cd $(PARSER_DIR); rm -f $(PARSER_GENSRCS)

.PHONY: clean-parser

clean-parser-objs:
	rm -f $(PARSER_OBJS)

.PHONY: clean-parser-objs

clean-sexp-objs:
	rm -f $(SEXP_OBJS)

.PNONY: clean-sexp-objs

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

objs: $(OBJS)

.PHONY: objs

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

###### Compiliing Sexp Sources ######

sexp-objs: $(SEXP_OBJS)

.PHONY: sexp-objs

$(SEXP_OBJS): | $(SEXP_OBJDIR)

$(SEXP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

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

$(PARSER_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

###### Compiling Test Executables #######

test-execs: $(TEST_EXECS)

.PHONY: test-execs

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

-include $(foreach dep,$(TEST_SRCS:.cpp=.d),$(TEST_EXECDIR)/$(dep))

$(TEST_EXECDIR)/TestParser: $(TEST_DIR)/TestParser.cpp $(PARSER_OBJS) \
                            $(SEXP_OBJS) $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(PARSER_OBJS) $(SEXP_OBJS) $(OBJS) -o $@
