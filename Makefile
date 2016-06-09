#
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

CXX = clang++

SRCDIR = src
OBJDIR = build/objs

SEXP_PARSER_SRCDIR = $(SRCDIR)/sexp-parser
SEXP_PARSER_OBJDIR = $(OBJDIR)/sexp-parser

SEXP_PARSER_GENSRCS = \
	location.hh \
        Parser.output \
	Parser.tab.hpp \
	Parser.tab.cpp \
	position.hh \
	stack.hh 

SEXP_PARSER_CLEAN_GEN=$(patsubst %, $(SEXP_PARSER_SRCDIR)/%, $(SEXP_PARSER_GENSRCS))

all: gen

clean: clean-gen
	rm -rf build

clean-gen:
	rm -rf $(SEXP_PARSER_CLEAN_GEN)

gen: sexp-parser

sexp-parser: $(SEXP_PARSER_SRCDIR)/Parser.ypp
	cd $(SEXP_PARSER_SRCDIR); bison -d -r all Parser.ypp

.PHONY: all clean clean-gen gen sexp-parser
