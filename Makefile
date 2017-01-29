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

# helper eq comparison function.
eq = $(and $(findstring $(1),$(2)),$(findstring $(2),$(1)))

include Makefile.common

GENSRCS =
GENSRCS_BOOT =

###### Utilities ######

UTILS_DIR = $(SRCDIR)/utils
UTILS_OBJDIR = $(OBJDIR)/utils
UTILS_OBJDIR_BOOT = $(OBJDIR_BOOT)/utils
UTILS_SRCS = \
	Allocator.cpp \
	ArgsParse.cpp \
	Defs.cpp \
	HuffmanEncoding.cpp \
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
	SectionSymbolTable.cpp

#	BinaryReader.cpp
#	BinaryWriter.cpp

BINARY_OBJS=$(patsubst %.cpp, $(BINARY_OBJDIR)/%.o, $(BINARY_SRCS))
BINARY_OBJS_BOOT=$(patsubst %.cpp, $(BINARY_OBJDIR_BOOT)/%.o, $(BINARY_SRCS))

BINARY_LIB = $(LIBDIR)/$(LIBPREFIX)binary.a
BINARY_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)binary.a

###### Parse objects and locations ######

PARSER_DIR = $(SRCDIR)/sexp-parser
PARSER_GENDIR = $(GENDIR)/sexp-parser
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
	Driver.cpp

PARSER_CPP_GENSRCS = \
	Lexer.cpp \
	Parser.tab.cpp

PARSER_GEN_SRCS = $(patsubst %, $(PARSER_GENDIR)/%, $(PARSER_GENSRCS))

GENSRCS += $(PARSER_GEN_SRCS)
GENSRCS_BOOT += $(PARSER_GEN_SRCS)

PARSER_STD_OBJS=$(patsubst %.cpp, $(PARSER_OBJDIR)/%.o, $(PARSER_SRCS))
PARSER_GEN_OBJS=$(patsubst %.cpp, $(PARSER_OBJDIR)/%.o, $(PARSER_CPP_GENSRCS))
PARSER_OBJS=$(PARSER_STD_OBJS) $(PARSER_GEN_OBJS)

PARSER_STD_OBJS_BOOT=$(patsubst %.cpp, $(PARSER_OBJDIR_BOOT)/%.o, $(PARSER_SRCS))
PARSER_GEN_OBJS_BOOT=$(patsubst %.cpp, $(PARSER_OBJDIR_BOOT)/%.o, $(PARSER_CPP_GENSRCS))
PARSER_OBJS_BOOT=$(PARSER_STD_OBJS_BOOT) $(PARSER_GEN_OBJS_BOOT)

PARSER_LIB = $(LIBDIR)/$(LIBPREFIX)parser.a
PARSER_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)parser.a

###### Filter s-expressions ######

SEXP_SRCDIR = $(SRCDIR)/sexp
SEXP_GENDIR = $(GENDIR)/sexp
SEXP_OBJDIR = $(OBJDIR)/sexp
SEXP_OBJDIR_BOOT = $(OBJDIR_BOOT)/sexp
SEXP_SRCS_BASE = \
	Ast.cpp \
	CasmReader.cpp \
	CasmWriter.cpp \
	FlattenAst.cpp \
	InflateAst.cpp \
	TextWriter.cpp

SEXP_SRCS = $(SEXP_SRCS_BASE)
SEXP_SRCS_BOOT = $(SEXP_SRCS_BASE)

SEXP_OBJS = $(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))
SEXP_OBJS_BOOT = $(patsubst %.cpp, $(SEXP_OBJDIR_BOOT)/%.o, $(SEXP_SRCS_BOOT))

SEXP_DEFAULT_DFS = defaults-0xd.df

SEXP_DEFAULT_ORIGSRCS = $(patsubst %.df, $(SEXP_SRCDIR)/%.df, $(SEXP_DEFAULT_DFS))

GENSRCS += $(SEXP_DEFAULT_SRCS)

SEXP_LIB = $(LIBDIR)/$(LIBPREFIX)sexp.a
SEXP_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)sexp.a

# This is the default file used by tests.

SEXP_DEFAULT_DF = $(SEXP_SRCDIR)/defaults-0xd.df
TEST_DEFAULT_DF = $(TEST_SRCS_DIR)/defaults-0xd.df
TEST_DEFAULT_WASM = $(TEST_SRCS_DIR)/defaults-0xd.wasm
TEST_DEFAULT_WASM_W = $(TEST_SRCS_DIR)/defaults-0xd.wasm-w

###### Default algorithms ######

ALG_SRCDIR = $(SRCDIR)/algorithms
ALG_GENDIR = $(GENDIR)/algorithms
ALG_OBJDIR = $(OBJDIR)/algorithms

ALG_SRCS = wasm0xd.cast casm0x0.cast

ALG_GEN_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cast, $(ALG_SRCS))

ALG_GEN_CPP_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cpp, $(ALG_SRCS))
ALG_GEN_H_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.h, $(ALG_SRCS))

GENSRCS += $(ALG_GEN_CPP_SRCS) $(ALG_GEN_H_SRCS)

ALG_OBJS = $(patsubst %.cast, $(ALG_OBJDIR)/%.o, $(ALG_SRCS))

ALG_LIB = $(LIBDIR)/$(LIBPREFIX)alg.a

ALG_GENDIR_ALG = $(ALG_GENDIR)/casm0x0.cast

###### Stream handlers ######

STRM_SRCDIR = $(SRCDIR)/stream
STRM_OBJDIR = $(OBJDIR)/stream
STRM_OBJDIR_BOOT = $(OBJDIR_BOOT)/stream

STRM_SRCS = \
	ArrayReader.cpp \
	BitReadCursor.cpp \
	BitWriteCursor.cpp \
	FileReader.cpp \
	FileWriter.cpp \
	Cursor.cpp \
	Page.cpp \
	Pipe.cpp \
	Queue.cpp \
	ReadCursor.cpp \
	ReadBackedQueue.cpp \
	StreamReader.cpp \
	StreamWriter.cpp \
	StringReader.cpp \
	StringWriter.cpp \
	WriteBackedQueue.cpp \
	WriteCursor.cpp \
	WriteCursorBase.cpp \
	WriteCursor2ReadQueue.cpp \
	WriteUtils.cpp

STRM_OBJS = $(patsubst %.cpp, $(STRM_OBJDIR)/%.o, $(STRM_SRCS))
STRM_OBJS_BOOT = $(patsubst %.cpp, $(STRM_OBJDIR_BOOT)/%.o, $(STRM_SRCS))

STRM_LIB = $(LIBDIR)/$(LIBPREFIX)strm.a
STRM_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)strm.a

###### S-expression interpeter ######

INTERP_SRCDIR = $(SRCDIR)/interp
INTERP_OBJDIR = $(OBJDIR)/interp
INTERP_OBJDIR_BOOT = $(OBJDIR_BOOT)/interp

INTERP_SRCS_BASE = \
	ByteReader.cpp \
	ByteReadStream.cpp \
	ByteWriter.cpp \
	ByteWriteStream.cpp \
	DecompressSelector.cpp \
	Interpreter.cpp \
	IntFormats.cpp \
	IntInterpreter.cpp \
	IntReader.cpp \
	IntStream.cpp \
	IntWriter.cpp \
	Reader.cpp \
	ReadStream.cpp \
	TeeWriter.cpp \
	Writer.cpp \
	WriteStream.cpp

INTERP_SRCS = $(INTERP_SRCS_BASE) Decompress.cpp

INTERP_OBJS = $(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))
INTERP_OBJS_BOOT = $(patsubst %.cpp, $(INTERP_OBJDIR_BOOT)/%.o, $(INTERP_SRCS_BASE))

INTERP_LIB = $(LIBDIR)/$(LIBPREFIX)interp.a
INTERP_LIB_BOOT = $(LIBDIR_BOOT)/$(LIBPREFIX)interp.a

###### Integer Compressor ######

INTCOMP_SRCDIR = $(SRCDIR)/intcomp
INTCOMP_OBJDIR = $(OBJDIR)/intcomp

INTCOMP_SRCS = \
	AbbrevAssignWriter.cpp \
	AbbreviationCodegen.cpp \
	AbbreviationsCollector.cpp \
	CountNodeCollector.cpp \
	CountWriter.cpp \
	IntCompress.cpp \
	IntCountNode.cpp \
	RemoveNodesVisitor.cpp

INTCOMP_OBJS = $(patsubst %.cpp, $(INTCOMP_OBJDIR)/%.o, $(INTCOMP_SRCS))
INTCOMP_LIB = $(LIBDIR)/$(LIBPREFIX)intcomp.a

###### Executables ######

EXEC_DIR = $(SRCDIR)/exec
EXEC_OBJDIR = $(OBJDIR)/exec
EXEC_OBJDIR_BOOT = $(OBJDIR_BOOT)/exec
BUILD_EXECDIR = $(BUILDDIR)/bin
BUILD_EXECDIR_BOOT = $(BUILDDIR_BOOT)/bin

EXEC_SRCS_BASE = \
	casm2cast.cpp \
	compress-int.cpp \
	decompress.cpp

EXEC_SRCS_BOOT = \
	cast2casm.cpp

EXEC_SRCS = $(EXEC_SRCS_BASE)  $(EXEC_SRCS_BOOT)

EXEC_OBJS = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS))

EXEC_OBJS_BOOT = $(patsubst %.cpp, $(EXEC_OBJDIR_BOOT)/%.o, $(EXEC_SRCS_BOOT))

EXECS = $(patsubst %.cpp, $(BUILD_EXECDIR)/%$(EXE), $(EXEC_SRCS))


EXECS_BOOT = $(patsubst %.cpp, $(BUILD_EXECDIR_BOOT)/%$(EXE_BOOT), $(EXEC_SRCS_BOOT))

###### Test executables and locations ######

TEST_DIR = $(SRCDIR)/test
TEST_OBJDIR = $(OBJDIR)/test
TEST_EXECDIR = $(BUILDDIR)/test

TEST_SRCS = \
	TestByteQueues.cpp \
	TestHuffman.cpp \
	TestParser.cpp \
	TestRawStreams.cpp

TEST_OBJS=$(patsubst %.cpp, $(TEST_OBJDIR)/%.o, $(TEST_SRCS))

TEST_EXECS=$(patsubst %.cpp, $(TEST_EXECDIR)/%$(EXE), $(TEST_SRCS))

###### Test sources and generated tests ######

TEST_SRCS_DIR = test/test-sources

TEST_0XD_SRCDIR = $(TEST_SRCS_DIR)/0xD
TEST_0XD_GENDIR = $(TEST_GENDIR)/0xD

TEST_WASM_SRCS = \
	address.wast \
	binary.wast \
	block.wast \
	br.wast \
	break-drop.wast \
	br_if.wast \
	br_table.wast \
	call_indirect.wast \
	call.wast \
	comments.wast \
	conversions.wast \
	endianness.wast \
	exports.wast \
	f32_cmp.wast \
	f32.wast \
	f64_cmp.wast \
	f64.wast \
	fac.wast \
	float_exprs.wast \
	float_literals.wast \
	float_memory.wast \
	float_misc.wast \
	forward.wast \
	func_ptrs.wast \
	func.wast \
	get_local.wast \
	globals.wast \
	i32.wast \
	i64.wast \
	imports.wast \
	int_exprs.wast \
	int_literals.wast \
	labels.wast \
	left-to-right.wast \
	linking.wast \
	loop.wast \
	memory_redundancy.wast \
	memory_trap.wast \
	memory.wast \
	names.wast \
	nop.wast \
	resizing.wast \
	return.wast \
	select.wast \
	set_local.wast \
	skip-stack-guard-page.wast \
	stack.wast \
	store_retval.wast \
	switch.wast \
	tee_local.wast \
	traps.wast \
	typecheck.wast \
	unreachable.wast \
	unwind.wast

WABT_WAST_DIR = $(WABT_DIR)/third_party/testsuite

TEST_WASM_SRC_FILES = $(patsubst %.wast, $(TEST_0XD_SRCDIR)/%.wasm, \
                        $(TEST_WASM_SRCS))

TEST_WASM_W_SRC_FILES = $(patsubst %.wast, $(TEST_0XD_SRCDIR)/%.wasm-w, \
                        $(TEST_WASM_SRCS))

# The following groups are for different types of tests on decompress.
TEST_WASM_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm, \
                       $(TEST_WASM_SRCS))

TEST_WASM_W_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-w, \
                       $(TEST_WASM_SRCS))

TEST_WASM_WS_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-ws, \
                       $(TEST_WASM_SRCS))

TEST_WASM_SW_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-sw, \
                       $(TEST_WASM_SRCS))

TEST_WASM_CAPI_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-capi, \
                        $(TEST_WASM_SRCS))

TEST_WASM_COMP_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-comp, \
                        $(TEST_WASM_SRCS))

TEST_CASM_SRCS = \
	defaults-0xd.df \
	ExprRedirects.df

TEST_CASM_SRC_FILES = $(patsubst %.df, $(TEST_SRCS_DIR)/%.df, \
			$(TEST_CASM_SRCS))

TEST_CASM_WASM_FILES = $(patsubst %.df, $(TEST_SRCS_DIR)/%.wasm, \
			$(TEST_CASM_SRCS))

TEST_CASM_WASM_W_FILES = $(patsubst %.df, $(TEST_SRCS_DIR)/%.wasm-w, \
			  $(TEST_CASM_SRCS))

TEST_CASM_DF_OUT_FILES = $(patsubst %.df, $(TEST_SRCS_DIR)/%.df-out, \
			  $(TEST_CASM_SRCS))

TEST_CASM_GEN_FILES = $(patsubst %.df, $(TEST_0XD_GENDIR)/%.wasm, \
	 	  	$(TEST_CASM_SRCS))

TEST_CASM_W_GEN_FILES = $(patsubst %.df, $(TEST_0XD_GENDIR)/%.wasm-w, \
	 	  	$(TEST_CASM_SRCS))

TEST_CASM_DF_GEN_FILES = $(patsubst %.df, $(TEST_0XD_GENDIR)/%.df-out, \
	 	  	$(TEST_CASM_SRCS))

###### General compilation definitions ######

LIBS = $(BINARY_LIB) $(INTERP_LIB) $(SEXP_LIB) $(PARSER_LIB) \
       $(STRM_LIB) $(INTCOMP_LIB) $(INTERP_LIB) $(BINARY_LIB) \
       $(ALG_LIB) $(SEXP_LIB) $(INTERP_LIB) $(BINARY_LIB) $(ALG_LIB) \
       $(STRM_LIB) $(UTILS_LIB) $(PARSER_LIB) 

LIBS_BOOT = $(BINARY_LIB_BOOT) $(INTERP_LIB_BOOT) \
	$(SEXP_LIB_BOOT) $(PARSER_LIB_BOOT) \
	$(INTERP_LIB_BOOT) $(BINARY_LIB_BOOT) $(STRM_LIB_BOOT) $(UTILS_LIB_BOOT)

##### Track additional important variable definitions not in Makefile.common

CCACHE := `command -v ccache`
CPP_COMPILER := CCACHE_CPP2=yes $(CCACHE) $(CXX)
CPP_COMPILER_BOOT := CCACHE_CPP2=yes $(CCACHE) $(CXX_BOOT)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
# Note: g++ on Travis doesn't support -std=gnu++11
CXXFLAGS_BASE := -Wall -Wextra -O2 -g -pedantic -MP -MD \
	    -Werror -Wno-unused-parameter -fno-omit-frame-pointer -fPIC \
	    -Isrc -I$(SRC_GENDIR)
CXXFLAGS := $(TARGET_CXXFLAGS) $(PLATFORM_CXXFLAGS) \
	    $(CXXFLAGS_BASE)

CXXFLAGS_BOOT := $(PLATFORM_CXXFLAGS_DEFAULT) $(CXXFLAGS_BASE) -DWASM_BOOT=1

ifneq ($(RELEASE), 0)
  CXXFLAGS += -DNDEBUG
endif

ifdef MAKE_PAGE_SIZE
  CXXFLAGS += -DWASM_DECODE_PAGE_SIZE=$(PAGE_SIZE)
endif

###### Default Rule ######

ifeq ($(UPDATE), 0)
  all: build-all
else
  all: update-all
endif

.PHONY: all

build-all: gen libs execs test-execs

###### Build submodules ######

wabt-submodule:
	cd $(WABT_DIR); git submodule update --init; make

.PHONY: wabt-submodule

###### Build boot executables ######

boot: $(EXECS_BOOT)

.PHONY: boot

###### Cleaning Rules #######

clean: clean-wabt
	rm -rf $(BUILDDIR) $(BUILDDIR_BOOT) $(GENDIR)

.PHONY: clean

clean-all: clean-wabt
	rm -rf $(BUILDBASEDIR) $(GENDIR)

.PHONY: clean-all

clean-wabt:
	cd $(WABT_DIR); make clean

.PHONY: clean-wabt

###### Source Generation Rules #######

gen: gen-parser gen-lexer boot

.PHONY: gen

gen-lexer: $(PARSER_GENDIR)/Lexer.cpp gen-parser

.PHONY: gen-lexer

gen-parser: $(PARSER_GENDIR)/Parser.tab.cpp

.PHONY: gen-parser

$(SEXP_DEFAULT_SRCS): | $(SEXP_GENDIR)

$(SEXP_GENDIR):
	mkdir -p $@

$(SEXP_DEFAULT_GENSRCS): $(SEXP_GENDIR)/%.df: $(SEXP_SRCDIR)/%.df
	cp $< $@

$(SEXP_DEFAULT_GENSRCS): | $(SEXP_GENDIR)

###### Compiliing binary generation Sources ######

$(ALG_OBJS): | $(ALG_OBJDIR)

$(ALG_OBJDIR):
	mkdir -p $@

$(ALG_GENDIR):
	mkdir -p $@

$(ALG_LIB): $(ALG_OBJS)
	ar -rs $@ $(ALG_OBJS)
	ranlib $@

$(ALG_GEN_SRCS): | $(ALG_GENDIR)

$(ALG_GEN_SRCS): $(ALG_GENDIR)/%.cast: $(ALG_SRCDIR)/%.cast
	cp $< $@

$(ALG_GEN_H_SRCS): $(ALG_GENDIR)/%.h: $(ALG_GENDIR)/%.cast \
		$(BUILD_EXECDIR_BOOT)/cast2casm
	$(BUILD_EXECDIR_BOOT)/cast2casm -a $(ALG_GENDIR_ALG) \
		$< -o $@ --header --function \
		$(patsubst $(ALG_GENDIR)/%.cast, getAlg%Symtab, $<)

$(ALG_GEN_CPP_SRCS): $(ALG_GENDIR)/%.cpp: $(ALG_GENDIR)/%.cast \
		$(BUILD_EXECDIR_BOOT)/cast2casm $(ALG_GENDIR_ALG)
	$(BUILD_EXECDIR_BOOT)/cast2casm -a $(ALG_GENDIR_ALG) \
		$< -o $@ --strip-literals --function \
		$(patsubst $(ALG_GENDIR)/%.cast, getAlg%Symtab, $<) \
		$(if $(call eq, "$(ALG_GENDIR)/casm0x0.cast", "$<") \
                      ,  , --strip-actions --array)

-include $(foreach dep,$(ALG_GEN_CPP_SRCS:.cpp=.d),$(ALG_OBJDIR)/$(dep))

$(ALG_OBJS): $(ALG_OBJDIR)/%.o: $(ALG_GENDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(BINARY_OBJS): | $(BINARY_OBJDIR)

$(BINARY_OBJDIR):
	mkdir -p $@

$(BINARY_OBJS_BOOT): | $(BINARY_OBJDIR_BOOT)

$(BINARY_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR)/$(dep))

$(BINARY_OBJS): $(BINARY_OBJDIR)/%.o: $(BINARY_DIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR_BOOT)/$(dep))

$(BINARY_OBJS_BOOT): $(BINARY_OBJDIR_BOOT)/%.o: $(BINARY_DIR)/%.cpp $(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(BINARY_LIB): $(BINARY_OBJS)
	ar -rs $@ $(BINARY_OBJS)
	ranlib $@

$(BINARY_LIB_BOOT): $(BINARY_OBJS_BOOT)
	ar -rs $@ $(BINARY_OBJS_BOOT)
	ranlib $@

###### Compiliing top-level Sources ######

$(UTILS_OBJS): | $(UTILS_OBJDIR)

$(UTILS_OBJDIR):
	mkdir -p $@

$(UTILS_OBJS_BOOT): | $(UTILS_OBJDIR_BOOT)

$(UTILS_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(UTILS_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR_BOOT)/$(dep))

$(UTILS_OBJS_BOOT): $(OBJDIR_BOOT)/%.o: $(SRCDIR)/%.cpp $(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(UTILS_LIB): $(UTILS_OBJS)
	ar -rs $@ $(UTILS_OBJS)
	ranlib $@

$(UTILS_LIB_BOOT): $(UTILS_OBJS_BOOT)
	ar -rs $@ $(UTILS_OBJS_BOOT)
	ranlib $@

###### Compiling s-expression interpeter sources ######

$(INTERP_OBJS): | $(INTERP_OBJDIR)

$(INTERP_OBJDIR):
	mkdir -p $@

$(INTERP_OBJS_BOOT): | $(INTERP_OBJDIR_BOOT)

$(INTERP_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR)/$(dep))

$(INTERP_OBJS): $(INTERP_OBJDIR)/%.o: $(INTERP_SRCDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR_BOOT)/$(dep))

$(INTERP_OBJS_BOOT): $(INTERP_OBJDIR_BOOT)/%.o: $(INTERP_SRCDIR)/%.cpp $(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(INTERP_LIB): $(INTERP_OBJS)
	ar -rs $@ $(INTERP_OBJS)
	ranlib $@

$(INTERP_LIB_BOOT): $(INTERP_OBJS_BOOT)
	ar -rs $@ $(INTERP_OBJS_BOOT)
	ranlib $@

###### Compiling the integer compressor sources ######

$(INTCOMP_OBJS): | $(INTCOMP_OBJDIR)

$(INTCOMP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(INTCOMP_SRCS:.cpp=.d),$(INTCOMP_OBJDIR)/$(dep))

$(INTCOMP_OBJS): $(INTCOMP_OBJDIR)/%.o: $(INTCOMP_SRCDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


$(INTCOMP_LIB): $(INTCOMP_OBJS)
	ar -rs $@ $(INTCOMP_OBJS)
	ranlib $@

###### Compiliing Sexp Sources ######

$(SEXP_OBJS): | $(SEXP_OBJDIR)

$(SEXP_OBJDIR):
	mkdir -p $@

$(SEXP_OBJS_BOOT): | $(SEXP_OBJDIR_BOOT)

$(SEXP_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_SRCDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR_BOOT)/$(dep))

$(SEXP_OBJS_BOOT): $(SEXP_OBJDIR_BOOT)/%.o: $(SEXP_SRCDIR)/%.cpp $(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

-include $(foreach dep,$(SEXP_DEFAULT_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_DEFAULT_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_GENDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(SEXP_LIB): $(SEXP_OBJS) $(SEXP_DEFAULT_OBJS)
	ar -rs $@ $(SEXP_OBJS) $(SEXP_DEFAULT_OBJS)
	ranlib $@

$(SEXP_LIB_BOOT): $(SEXP_OBJS_BOOT)
	ar -rs $@ $(SEXP_OBJS_BOOT)
	ranlib $@

###### Compiling stream sources ######

$(STRM_OBJS): | $(STRM_OBJDIR)

$(STRM_OBJDIR):
	mkdir -p $@

$(STRM_OBJS_BOOT): | $(STRM_OBJDIR_BOOT)

$(STRM_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR)/$(dep))

$(STRM_OBJS): $(STRM_OBJDIR)/%.o: $(STRM_SRCDIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR_BOOT)/$(dep))

$(STRM_OBJS_BOOT): $(STRM_OBJDIR_BOOT)/%.o: $(STRM_SRCDIR)/%.cpp $(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(STRM_LIB): $(STRM_OBJS)
	ar -rs $@ $(STRM_OBJS)
	ranlib $@

$(STRM_LIB_BOOT): $(STRM_OBJS_BOOT)
	ar -rs $@ $(STRM_OBJS_BOOT)
	ranlib $@

###### Compiling Filter Parser #######

$(PARSER_GENDIR):
	mkdir -p $@

$(PARSER_GENDIR)/Lexer.lex: $(PARSER_DIR)/Lexer.lex $(PARSER_GENDIR)
	cp $< $@

-include $(PARSER_GENDIR)/Lexer.d

$(PARSER_GENDIR)/Lexer.cpp: $(PARSER_GENDIR)/Lexer.lex
	cd $(PARSER_GENDIR); lex -o Lexer.cpp Lexer.lex

$(PARSER_GENDIR)/Parser.ypp: $(PARSER_DIR)/Parser.ypp $(PARSER_GENDIR)
	cp $< $@

-include $(PARSER_GENDIR)/Parser.tab.d

$(PARSER_GENDIR)/Parser.tab.cpp: $(PARSER_GENDIR)/Parser.ypp
	cd $(PARSER_GENDIR); bison -d -r all Parser.ypp

$(PARSER_GENDIR)/location.hh: $(PARSER_GENDIR)/Parser.tab.cpp
	touch $@

$(PARSER_GENDIR)/Parser.output: $(PARSER_GENDIR)/Parser.tab.cpp
	touch $@

$(PARSER_GENDIR)/Parser.tab.hpp: $(PARSER_GENDIR)/Parser.tab.cpp
	touch $@


$(PARSER_GENDIR)/position.hh: $(PARSER_GENDIR)/Parser.tab.cpp
	touch $@

$(PARSER_GENDIR)/stack.hh: $(PARSER_GENDIR)/Parser.tab.cpp
	touch $@

$(PARSER_OBJS): | $(PARSER_OBJDIR)

$(PARSER_OBJDIR):
	mkdir -p $@

$(PARSER_OBJS_BOOT): | $(PARSER_OBJDIR_BOOT)

$(PARSER_OBJDIR_BOOT):
	mkdir -p $@

-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_STD_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(PARSER_CPP_GENSRCS:.cpp=.d),$(PARSER_OBJDIR)/$(dep))

$(PARSER_GEN_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_GENDIR)/%.cpp \
		$(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(PARSER_SRCS:.cpp=.d),$(PARSER_OBJDIR_BOOT)/$(dep))

$(PARSER_STD_OBJS_BOOT): $(PARSER_OBJDIR_BOOT)/%.o: $(PARSER_DIR)/%.cpp \
		$(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

-include $(foreach dep,$(PARSER_CPP_GENSRCS:.cpp=.d),$(PARSER_OBJDIR_BOOT)/$(dep))

$(PARSER_GEN_OBJS_BOOT): $(PARSER_OBJDIR_BOOT)/%.o: $(PARSER_GENDIR)/%.cpp \
		$(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(PARSER_LIB): $(PARSER_OBJS)
	ar -rs $@ $(PARSER_OBJS)
	ranlib $@

$(PARSER_LIB_BOOT): $(PARSER_OBJS_BOOT)
	ar -rs $@ $(PARSER_OBJS_BOOT)
	ranlib $@

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

execs: $(LIBS_BOOT) $(EXECS_BOOT) $(LIBS) $(EXECS) $(EXECS_BOOT)

.PHONY: execs

$(EXEC_OBJDIR):
	mkdir -p $@

$(EXEC_OBJS): | $(EXEC_OBJDIR)

$(EXEC_OBJDIR_BOOT):
	mkdir -p $@

$(EXEC_OBJS_BOOT): | $(EXEC_OBJDIR_BOOT)

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS): $(EXEC_OBJDIR)/%.o: $(EXEC_DIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR_BOOT)/$(dep))

$(EXEC_OBJS_BOOT): $(EXEC_OBJDIR_BOOT)/%.o: $(EXEC_DIR)/%.cpp \
		$(GENSRCS_BOOT)
	$(CPP_COMPILER_BOOT) -c $(CXXFLAGS_BOOT) $< -o $@

$(BUILD_EXECDIR):
	mkdir -p $@

$(BUILD_EXECDIR_BOOT):
	mkdir -p $@

$(EXECS): | $(BUILD_EXECDIR)

$(EXECS): $(BUILD_EXECDIR)/%$(EXE): $(EXEC_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

$(EXECS_BOOT): | $(BUILD_EXECDIR_BOOT)

$(EXECS_BOOT): $(BUILD_EXECDIR_BOOT)/%$(EXE_BOOT): $(EXEC_OBJDIR_BOOT)/%.o $(LIBS_BOOT)
	$(CPP_COMPILER_BOOT) $(CXXFLAGS_BOOT) $< $(LIBS_BOOT) -o $@

###### Compiling Test Executables #######

test-execs: $(TEST_EXECS)

.PHONY: test-execs

$(TEST_OBJDIR):
	mkdir -p $@

$(TEST_OBJS): | $(TEST_OBJDIR)

-include $(foreach dep,$(TEST_SRCS:.cpp=.d),$(TEST_OBJDIR)/$(dep))

$(TEST_OBJS): $(TEST_OBJDIR)/%.o: $(TEST_DIR)/%.cpp $(GENSRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(TEST_EXECDIR):
	mkdir -p $@

$(TEST_EXECS): | $(TEST_EXECDIR)

$(TEST_EXECS): $(TEST_EXECDIR)/%$(EXE): $(TEST_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Testing ######

test: build-all test-parser test-raw-streams test-byte-queues \
	test-huffman test-decompress test-casm2cast test-cast2casm \
	test-casm-cast test-compress 
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

test-huffman: $(TEST_EXECDIR)/TestHuffman
	$< | diff - $(TEST_SRCS_DIR)/TestHuffman.out
	@echo "*** Huffman encoding tests passed ***"

.PHONY: test-huffman

test-decompress: \
	$(TEST_WASM_GEN_FILES) \
	$(TEST_WASM_W_GEN_FILES) \
	$(TEST_WASM_CAPI_GEN_FILES) \
	$(TEST_WASM_WS_GEN_FILES) \
	$(TEST_WASM_SW_GEN_FILES)
	@echo "*** decompress 0xD tests passed ***"

.PHONY: test-decompress

test-compress: $(TEST_WASM_COMP_FILES)
	@echo "*** compress 0xD tests passed ***"

.PHONY: test-compress

$(TEST_CASM_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm: $(TEST_SRCS_DIR)/%.df \
		$(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< | \
		cmp - $(patsubst %.df, %.wasm, $<)

.PHONY: $(TEST_CASM_GEN_FILES)

$(TEST_CASM_W_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-w: $(TEST_SRCS_DIR)/%.df \
		$(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm -m $< | \
		cmp - $(patsubst %.df, %.wasm-w, $<)

.PHONY: $(TEST_CASM_W_GEN_FILES)

$(TEST_CASM_DF_GEN_FILES): $(TEST_0XD_GENDIR)/%.df-out: $(TEST_SRCS_DIR)/%.wasm \
		$(TEST_SRCS_DIR)/%.wasm $(BUILD_EXECDIR)/casm2cast
	$(BUILD_EXECDIR)/casm2cast $< | \
		cmp - $(patsubst %.wasm, %.df-out, $<)
	$(BUILD_EXECDIR)/casm2cast \
		$(patsubst %.wasm, %.wasm-w, $<) | \
		cmp - $(patsubst %.wasm, %.df-out, $<)

.PHONY: $(TEST_CASM_DF_GEN_FILES)

# Note: Currently only tests that code executes (without errors).
$(TEST_WASM_COMP_FILES): $(TEST_0XD_GENDIR)/%.wasm-comp: $(TEST_0XD_SRCDIR)/%.wasm \
		$(BUILD_EXECDIR)/compress-int $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/compress-int --min-int-count 2 --min-weight 5 $< \
	| $(BUILD_EXECDIR)/decompress - | cmp - $<

# TODO(karlschimpf) Turn test back on when binary bit reading/writing working.
#	$(BUILD_EXECDIR)/compress-int --Huffman --min-int-count 2 --min-weight 5 $< \
#	| $(BUILD_EXECDIR)/decompress - | cmp - $<

.PHONY: $(TEST_WASM_COMP_FILES)

$(TEST_WASM_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm: $(TEST_0XD_SRCDIR)/%.wasm \
		$(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress $< | cmp - $<

.PHONY: $(TEST_WASM_GEN_FILES)

$(TEST_WASM_W_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-w: \
		$(TEST_0XD_SRCDIR)/%.wasm-w $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress -m $< | cmp - $<

.PHONY: $(TEST_WASM_W_GEN_FILES)


$(TEST_WASM_WS_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-ws: \
		$(TEST_0XD_SRCDIR)/%.wasm $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress $<-w | cmp - $<

.PHONY: $(TEST_WASM_WS_GEN_FILES)


$(TEST_WASM_SW_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-sw: \
		$(TEST_0XD_SRCDIR)/%.wasm $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress -m $< | cmp - $<-w

.PHONY: $(TEST_WASM_WS_GEN_FILES)

$(TEST_WASM_WPD_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-wpd: \
		$(TEST_0XD_SRCDIR)/%.wasm-w \
		$(BUILD_EXECDIR)/decompress $(SEXP_DEFAULT_GENSRCS)
	$(BUILD_EXECDIR)/decompress -p -d $(SEXP_GENDIR)/defaults-0xd.df \
		 $< | cmp - $<

.PHOHY: $(TEST_WASM_WPD_GEN_FILES)


$(TEST_WASM_CAPI_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-capi: \
		$(TEST_0XD_SRCDIR)/%.wasm-w $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress --c-api $< | cmp - $<

.PHOHY: $(TEST_WASM_WPD_GEN_FILES)

test-cast2casm: $(TEST_CASM_GEN_FILES) $(TEST_CASM_W_GEN_FILES)
	@echo "*** cast2casm tests passed ***"

.PHONY: test-cast2cast

test-casm2cast: $(BUILD_EXECDIR)/casm2cast $(TEST_CASM_DF_GEN_FILES) 
	$< $(TEST_DEFAULT_WASM) | diff - $(TEST_DEFAULT_DF)
	$< $(TEST_DEFAULT_WASM_W) | diff - $(TEST_DEFAULT_DF)
	@echo "*** casm2cast tests passed ***"

.PHONY: test-casm2cast

test-casm-cast: $(BUILD_EXECDIR)/cast2casm $(BUILD_EXECDIR)/casm2cast \
		$(TEST_SRCS_DIR)/BinaryFormat.cast
	$(BUILD_EXECDIR)/cast2casm $(TEST_SRCS_DIR)/BinaryFormat.cast | \
	$(BUILD_EXECDIR)/casm2cast - | \
	diff -  $(TEST_SRCS_DIR)/BinaryFormat.cast-out
	$(BUILD_EXECDIR)/cast2casm $(TEST_SRCS_DIR)/BinaryFormat.cast-out | \
	$(BUILD_EXECDIR)/casm2cast - | \
	diff -  $(TEST_SRCS_DIR)/BinaryFormat.cast-out

.PHONY: test-casm-cast

test-parser: $(TEST_EXECDIR)/TestParser
	$< -w $(SEXP_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -w $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< --expect-fail $(TEST_SRCS_DIR)/MismatchedParens.df 2>&1 | \
		diff - $(TEST_SRCS_DIR)/MismatchedParens.df-out
	$< -w $(TEST_SRCS_DIR)/ExprRedirects.df | \
		diff - $(TEST_SRCS_DIR)/ExprRedirects.df-out
	$< -w $(TEST_SRCS_DIR)/BinaryFormat.cast | \
		diff - $(TEST_SRCS_DIR)/BinaryFormat.cast-out
	@echo "*** parser tests passed ***"

.PHONY: test-parser

test-raw-streams: $(TEST_EXECDIR)/TestRawStreams
	$< -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -i $(TEST_DEFAULT_DF) -s | diff - $(TEST_DEFAULT_DF)
	@echo "*** test raw streams passed ***"

.PHONY: test-raw-streams

test-byte-queues: $(TEST_EXECDIR)/TestByteQueues
	$< -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 2 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 3 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 4 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 5 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 6 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 7 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 13 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 119 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 2323 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
	$< -c 3231 -i $(TEST_DEFAULT_DF) | diff - $(TEST_DEFAULT_DF)
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

$(UNITTEST_EXECS): $(UNITTEST_EXECDIR)/%$(EXE): $(UNITTEST_DIR)/%.cpp $(GENSRCS) $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) -I$(GTEST_INCLUDE) $< $(LIBS) \
		$(GTEST_LIB) -lpthread -o $@

$(UNITTEST_TESTS): $(UNITTEST_EXECDIR)/%.test: $(UNITTEST_EXECDIR)/%
	$<

unit-tests: $(UNITTEST_TESTS)

.PHONY: unit-tests $(UNITTEST_TESTS)

clean-unit-tests:
	rm -rf $(UNITTEST_EXECDIR)/*

###### Updating tests sources ######

ifneq ($(UPDATE), 0)

update-all: wabt-submodule gen \
	$(TEST_WASM_SRC_FILES) \
	$(TEST_WASM_W_SRC_FILES) \
	$(TEST_DEFAULT_DF) \
	$(TEST_CASM_WASM_FILES) \
	$(TEST_CASM_WASM_W_FILES) \
	$(TEST_CASM_DF_OUT_FILES)

$(TEST_WASM_SRC_FILES): $(TEST_0XD_SRCDIR)/%.wasm: $(WABT_WAST_DIR)/%.wast \
		wabt-submodule
	$(WABT_DIR)/out/wast2wasm -o $@ $<

$(TEST_WASM_W_SRC_FILES): $(TEST_0XD_SRCDIR)/%.wasm-w: $(WABT_WAST_DIR)/%.wast \
		wabt-submodule
	$(WABT_DIR)/out/wast2wasm --no-canonicalize-leb128s -o $@ $<

$(TEST_DEFAULT_DF): $(TEST_SRCS_DIR)/%.df: $(SEXP_SRCDIR)/%.df \
		 $(TEST_EXECDIR)/TestParser
	rm -rf $@; $(TEST_EXECDIR)/TestParser -w $< > $@

$(TEST_CASM_WASM_FILES): $(TEST_SRCS_DIR)/%.wasm: $(TEST_SRCS_DIR)/%.df \
		$(BUILD_EXECDIR)/cast2casm
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< -o $@

$(TEST_CASM_WASM_W_FILES): $(TEST_SRCS_DIR)/%.wasm-w: $(TEST_SRCS_DIR)/%.df \
		$(BUILD_EXECDIR)/cast2casm
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm -m $< -o $@

$(TEST_CASM_DF_OUT_FILES): $(TEST_SRCS_DIR)/%.df-out: $(TEST_SRCS_DIR)/%.wasm \
		$(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/casm2cast $< -o $@

endif

