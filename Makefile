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

CPP_COMPILER ?= clang++

SRCDIR = src
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

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

all: gen parser_objs test_execs

###### Cleaning Rules #######

clean: clean-parser

clean-parser:
	cd $(PARSER_DIR); rm -f $(PARSER_GENSRCS)

###### Source Generation Rules #######

gen: gen-parser gen-lexer

gen-lexer: $(PARSER_DIR)/Lexer.cpp

###### Compiling Filter Parser #######

$(PARSER_DIR)/Lexer.cpp: $(PARSER_DIR)/Lexer.lex
	cd $(PARSER_DIR); lex -o Lexer.cpp Lexer.lex

gen-parser: $(PARSER_DIR)/Parser.tab.cpp

$(PARSER_DIR)/Parser.tab.cpp: $(PARSER_DIR)/Parser.ypp
	cd $(PARSER_DIR); bison -d -r all Parser.ypp

parser_objs: $(PARSER_OBJS)

$(PARSER_OBJS): | $(PARSER_OBJDIR)

$(PARSER_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

###### Compiling Test Executables #######

test_execs: $(TEST_EXECS)

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

$(TEST_EXECDIR)/TestParser: $(TEST_DIR)/TestParser.cpp $(PARSER_OBJS)
	$(CXX) $(CXXFLAGS) $< $(PARSER_OBJS) -o $@

.PHONY: all clean clean-parser gen gen-lexer gen-parser parser_objs
.PHONY: test_execs

