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

SRCDIR = src
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

SEXP_PARSERDIR = $(SRCDIR)/sexp-parser
SEXP_PARSEROBJDIR = $(OBJDIR)/sexp-parser
SEXP_PARSER_GENSRCS = \
	location.hh \
	Lexer.cpp \
	Parser.output \
	Parser.tab.cpp \
	Parser.tab.hpp \
	position.hh \
	stack.hh

all: gen

clean: clean-parser

clean-parser:
	cd $(SEXP_PARSERDIR); rm -f $(SEXP_PARSER_GENSRCS)

gen: gen-parser gen-lexer

gen-lexer: $(SEXP_PARSERDIR)/Lexer.lex
	cd $(SEXP_PARSERDIR); lex -o Lexer.cpp Lexer.lex

gen-parser: $(SEXP_PARSERDIR)/Parser.ypp
	cd $(SEXP_PARSERDIR); bison -d -r all Parser.ypp

PHONY: all clean clean-parser gen gen-parser



