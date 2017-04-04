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

# Generate boot sources: make gen
#
# Build: make FLAGS
#
# FLAGS is defined as follows:
#     missing : (default) build debug.
#     DEBUG=1 : build debug
#     RELEASE=1 : build release
#
# To test: make FLAGS test
#
# To generate sourcess, build, and test: make FLAGS all
#
# To get rid of objects/libraries: make FLAGS clean
#
# To get rid of generated sources also: make clean-all

include Makefile.common

###### Sources generated during "make gen" #####
#
# Define explicit build rule dependencies, based on the following
# sequence of steps:

# Generated source that needs to be built before compiling any code.
GENERATED_COPY_SOURCES =
GENERATED_PARSE_SOURCES =
# The first set of boot executables.
GENERATED_BOOT1_OBJS =
GENERATED_BOOT1_LIBS =
GENERATED_BOOT1_EXECS =
GENERATED_BOOT1_SOURCES =
# The second set of boot executables.
GENERATED_BOOT2_OBJS =
GENERATED_BOOT2_LIBS =
GENERATED_BOOT2_EXECS =
GENERATED_BOOT2_SOURCES =
# Everything else to do after boot (i.e. make gen) is complete.
GENERATED_REST_OBJS =
GENERATED_REST_LIBS =
GENERATED_REST_EXECS =

###### Utilities ######

UTILS_DIR = $(SRCDIR)/utils
UTILS_OBJDIR = $(OBJDIR)/utils
UTILS_SRCS = \
	Allocator.cpp \
	ArgsParse.cpp \
	ArgsParseBool.cpp \
	ArgsParseCharstring.cpp \
	ArgsParseSize_t.cpp \
	ArgsParseString.cpp \
	ArgsParseInt32_t.cpp \
	ArgsParseInt64_t.cpp \
	ArgsParseUint32_t.cpp \
	ArgsParseUint64_t.cpp \
	Defs.cpp \
	HuffmanEncoding.cpp \
	Trace.cpp

UTILS_OBJS=$(patsubst %.cpp, $(UTILS_OBJDIR)/%.o, $(UTILS_SRCS))
UTILS_LIB = $(LIBDIR)/$(LIBPREFIX)utis.a

GENERATED_BOOT1_OBJS += $(UTIL_OBJS)
GENERATED_BOOT1_LIBS += $(UTILS_LIB)

###### Binary generation objects and locations ######

BINARY_DIR = $(SRCDIR)/binary
BINARY_OBJDIR = $(OBJDIR)/binary
BINARY_SRCS = \
	SectionSymbolTable.cpp

BINARY_OBJS=$(patsubst %.cpp, $(BINARY_OBJDIR)/%.o, $(BINARY_SRCS))
BINARY_LIB = $(LIBDIR)/$(LIBPREFIX)binary.a

GENERATED_BOOT1_OBJS += $(BINARY_OBJS)
GENERATED_BOOT1_LIBS += $(BINARY_LIB)

###### Parse objects and locations ######

PARSER_DIR = $(SRCDIR)/sexp-parser
PARSER_GENDIR = $(GENDIR)/sexp-parser
PARSER_OBJDIR = $(OBJDIR)/sexp-parser
PARSER_SRCS =\
	Parser.ypp \
	Lexer.lex

PARSER_LEX_GENSRCS = Lexer.cpp

PARSER_PARSE_CPP_GENSRCS = \
	Parser.tab.cpp

PARSER_PARSE_OTHER_GENSRCS = \
	location.hh \
	Parser.output \
	Parser.tab.hpp \
	position.hh \
	stack.hh

PARSER_GENSRCS = $(PARSER_LEX_GENSRCS) $(PARSER_PARSE_CPP_GENSRCS)

PARSER_SRCS_REST = \
	Driver.cpp

PARSER_CPP_GENSRCS = $(filter %.cpp, $(PARSER_GENSRCS))
PARSER_COPY_SRCS = $(patsubst %, $(PARSER_GENDIR)/%, $(PARSER_SRCS))
PARSER_GEN_SRCS = $(patsubst %, $(PARSER_GENDIR)/%, $(PARSER_GENSRCS))

PARSER_REST_OBJS=$(patsubst %.cpp, $(PARSER_OBJDIR)/%.o, $(PARSER_SRCS_REST))
PARSER_GEN_OBJS=$(patsubst %.cpp, $(PARSER_OBJDIR)/%.o, $(PARSER_CPP_GENSRCS))

PARSER_OBJS=$(PARSER_REST_OBJS) $(PARSER_GEN_OBJS)
PARSER_LIB = $(LIBDIR)/$(LIBPREFIX)parser.a

GENERATED_COPY_SOURCES += $(PARSER_COPY_SRCS)
GENERATED_PARSE_SOURCES += $(PARSER_GEN_SRCS)
GENERATED_BOOT1_OBJS += $(PARSER_OBJS)
GENERATED_BOOT1_LIBS += $(PARSER_LIB)

###### s-expression representation  ######

SEXP_SRCDIR = $(SRCDIR)/sexp
SEXP_GENDIR = $(GENDIR)/sexp
SEXP_OBJDIR = $(OBJDIR)/sexp
SEXP_SRCS = \
	Ast.cpp \
	TextWriter.cpp

SEXP_OBJS = $(patsubst %.cpp, $(SEXP_OBJDIR)/%.o, $(SEXP_SRCS))

SEXP_LIB = $(LIBDIR)/$(LIBPREFIX)sexp.a

GENERATED_BOOT1_OBJS += $(SEXP_OBJS)
GENERATED_BOOT1_LIBS += $(SEXP_LIB)

###### Casm algorithsm/file handling. ######

CASM_SRCDIR = $(SRCDIR)/casm
CASM_GENDIR = $(GENDIR)/casm
CASM_OBJDIR = $(OBJDIR)/casm

CASM_SRCS_BASE = \
	CasmReader.cpp \
	CasmWriter.cpp \
	FlattenAst.cpp \
	InflateAst.cpp

CASM_OBJS_BASE = $(patsubst %.cpp, $(CASM_OBJDIR)/%.o, $(CASM_SRCS_BASE))
CASM_LIB_BASE = $(LIBDIR)/$(LIBPREFIX)casm-base.a

# These files are in a separate directory because they require the C++
# source files for CASM.
CASM_SRCS_BIN = \
	CasmReaderBinary.cpp \
	CasmWriterBinary.cpp
CASM_OBJS_BIN = $(patsubst %.cpp, $(CASM_OBJDIR)/%.o, $(CASM_SRCS_BIN))
CASM_LIB_BIN = $(LIBDIR)/$(LIBPREFIX)casm-bin.a

CASM_SRCS = $(CASM_SRCS_BASE) $(CASM_SRCS_BIN)
CASM_OBJS = $(CASM_OBJS_BASE) $(CASM_OBJS_BIN)
CASM_LIB = $(CASM_LIB_BIN) $(CASM_LIB_BASE)

GENERATED_BOOT2_OBJS += $(CASM_OBJS_BASE)
GENERATED_BOOT2_LIBS += $(CASM_LIBS_BASE)

GENERATED_REST_OBJS += $(CASM_OBJS_BIN)
GENERATED_REST_LIBS += $(CASM_LIBS_BIN)

#######
# This is the default file used by tests.

TEST_DEFAULT_CAST = $(TEST_SRCS_DIR)/Wasm0xd.cast
TEST_DEFAULT_CAST_OUT = $(TEST_SRCS_DIR)/Wasm0xd.cast-out
TEST_DEFAULT_CASM = $(TEST_SRCS_DIR)/Wasm0xd.casm
TEST_DEFAULT_CASM_M = $(TEST_SRCS_DIR)/Wasm0xd.casm-m

###### Default algorithms ######

ALG_SRCDIR = $(SRCDIR)/algorithms
ALG_GENDIR = $(GENDIR)/algorithms
ALG_OBJDIR = $(OBJDIR)/algorithms

# helper function for extracing algorithm name from filename
alg_name = $(patsubst $(ALG_GENDIR)/%.cast, Alg%, $(1))

ALG_CAST = casm0x0.cast
ALG_GENDIR_ALG = $(ALG_GENDIR)/casm0x0.cast

#### Boot step 1

ALG_CAST_BOOT = casm0x0Boot.cast

ALG_CAST_BOOT1 = $(ALG_CAST_BOOT) $(ALG_CAST)

ALG_BOOT1_CAST_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cast, $(ALG_CAST_BOOT))
ALG_BOOT1_CAST_LIT_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%-lits.cast, \
				$(ALG_CAST))
ALG_BOOT1_H_SRCS = $(patsubst %.cast, %.h, $(ALG_BOOT1_CAST_SRCS))
ALG_BOOT1_CPP_SRCS = $(patsubst %.cast, %.cpp, $(ALG_BOOT1_CAST_SRCS))
ALG_BOOT1_LIT_H_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%-lits.h, $(ALG_CAST))
ALG_BOOT1_LIT_CPP_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%-lits.cpp, $(ALG_CAST))
ALG_BOOT1_SRCS = $(ALG_BOOT1_H_SRCS) $(ALG_BOOT1_LIT_CPP_SRCS) \
			$(ALG_BOOT1_CPP_SRCS) $(ALG_BOOT1_LIT_H_SRCS)

ALG_CAST_SRCS += $(ALG_BOOT1_CAST_SRCS)
ALG_CAST_LIT_SRCS += $(ALG_BOOT1_CAST_LIT_SRCS)

GENERATED_COPY_SOURCES += $(ALG_BOOT1_CAST_SRCS)
GENERATED_BOOT1_SOURCES += $(ALG_BOOT1_SRCS)

#### Boot step 2

ALG_CAST_BOOT2_BASE = wasm0xd.cast cism0x0.cast
ALG_CAST_BOOT2 = $(ALG_CAST) $(ALG_CAST_BOOT2_BASE)
ALG_BOOT2_CAST_BASE_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cast, $(ALG_CAST_BOOT2_BASE))
ALG_BOOT2_CAST_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cast, $(ALG_CAST_BOOT2))
ALG_BOOT2_CAST_LIT_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%-lits.cast, \
				$(ALG_CAST_BOOT2_BASE))
ALG_BOOT2_H_SRCS = $(patsubst %.cast, %.h, $(ALG_BOOT2_CAST_SRCS))
ALG_BOOT2_CPP_SRCS = $(patsubst %.cast, %.cpp, $(ALG_BOOT2_CAST_SRCS))
ALG_BOOT2_SRCS = $(ALG_BOOT2_H_SRCS) $(ALG_BOOT2_CPP_SRCS)

ALG_CAST_SRCS += $(ALG_BOOT2_CAST_SRCS)
ALG_CAST_LIT_SRCS += $(ALG_BOOT2_CAST_LIT_SRCS)

ALG_BOOT2_OBJS = $(patsubst $(ALG_GENDIR)/%.cpp, \
				$(ALG_OBJDIR)/%.o, \
				$(ALG_BOOT1_CPP_SRCS))
ALG_BOOT2_LIB = $(LIBDIR)/$(LIBPREFIX)alg-boot2.a

GENERATED_BOOT2_SOURCES = $(ALG_BOOT2_SRCS)
GENERATED_BOOT2_LIBS += $(ALG_BOOT2_LIB)

#### Build step

ALG_SRCS = $(ALG_SRCS_BOOT1) $(ALG_SRCS_BOOT2)
ALG_GEN_SRCS = $(patsubst %.cast, $(ALG_GENDIR)/%.cast, $(ALG_SRCS))
ALG_REST_OBJS = $(patsubst $(ALG_GENDIR)/%.cpp, \
				$(ALG_OBJDIR)/%.o, \
				$(ALG_BOOT2_CPP_SRCS))
ALG_LIB = $(LIBDIR)/$(LIBPREFIX)alg.a

ALG_OBJS = $(ALG_BOOT2_OBJS) $(ALG_REST_OBJS)

GENERATED_COPY_SOURCES += $(ALG_CAST_SRCS)

GENERATED_REST_OBJS += $(ALG_BOOT2_OBJS)
GENERATED_REST_LIBS += $(ALG_LIB)

###### Stream handlers ######

STRM_SRCDIR = $(SRCDIR)/stream
STRM_OBJDIR = $(OBJDIR)/stream

STRM_SRCS = \
	ArrayReader.cpp \
	BitReadCursor.cpp \
	BitWriteCursor.cpp \
	BlockEob.cpp \
	FileReader.cpp \
	FileWriter.cpp \
	Cursor.cpp \
	Page.cpp \
	PageCursor.cpp \
	Pipe.cpp \
	Queue.cpp \
	ReadCursor.cpp \
	ReadCursorFormatHelpers.cpp \
	ReadBackedQueue.cpp \
	StringReader.cpp \
	StringWriter.cpp \
	WriteBackedQueue.cpp \
	WriteCursor.cpp \
	WriteCursorFormatHelpers.cpp \
	WriteCursorBase.cpp \
	WriteCursor2ReadQueue.cpp \
	WriteUtils.cpp

STRM_OBJS = $(patsubst %.cpp, $(STRM_OBJDIR)/%.o, $(STRM_SRCS))
STRM_LIB = $(LIBDIR)/$(LIBPREFIX)strm.a

GENERATED_BOOT1_OBJS += $(STRM_OBJS)
GENERATED_BOOT1_LIBS += $(STRM_LIB)

###### S-expression interpeter ######

INTERP_SRCDIR = $(SRCDIR)/interp
INTERP_OBJDIR = $(OBJDIR)/interp

INTERP_SRCS_BASE = \
	AlgorithmSelector.cpp \
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

INTERP_OBJS_BASE = $(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS_BASE))
INTERP_LIB_BASE = $(LIBDIR)/$(LIBPREFIX)interp-base.a

INTERP_SRCS_C = Decompress.cpp
INTERP_OBJS_C = $(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS_C))
INTERP_LIB_C = $(LIBDIR)/$(LIBPREFIX)interp-c.a

INTERP_SRCS = $(INTERP_SRCS_BASE) $(INTERP_SRCS_C)
INTERP_OBJS = $(patsubst %.cpp, $(INTERP_OBJDIR)/%.o, $(INTERP_SRCS))
INTERP_LIB = $(INTERP_LIB_C) $(INTERP_LIB_BASE)

GENERATED_BOOT2_OBJS += $(INTERP_OBJS_BASE)
GENERATED_BOOT2_LIBS += $(INTERP_LIB_BASE)
GENERATED_REST_OBJS += $(INTERP_OBJS_C)
GENERATED_REST_LIBS += $(INTERP_LIB_C)

###### Integer Compressor ######

INTCOMP_SRCDIR = $(SRCDIR)/intcomp
INTCOMP_OBJDIR = $(OBJDIR)/intcomp

INTCOMP_SRCS = \
	AbbrevAssignWriter.cpp \
	AbbreviationCodegen.cpp \
	AbbreviationsCollector.cpp \
	AbbrevSelector.cpp \
	CompressionFlags.cpp \
	CountNode.cpp \
	CountNodeVisitor.cpp \
	CountNodeCollector.cpp \
	CountWriter.cpp \
	IntCompress.cpp \
	RemoveNodesVisitor.cpp

INTCOMP_OBJS = $(patsubst %.cpp, $(INTCOMP_OBJDIR)/%.o, $(INTCOMP_SRCS))
INTCOMP_LIB = $(LIBDIR)/$(LIBPREFIX)intcomp.a

GENERATED_REST_OBJS += $(INTCOMP_OBJS)
GENERATED_REST_LIBS += $(INTCOMP_LIB)

###### Executables ######

EXECDIR = $(SRCDIR)/exec
EXEC_OBJDIR = $(OBJDIR)/exec
BUILD_EXECDIR = $(BUILDDIR)/bin

EXEC_OBJDIR_BOOT = $(EXEC_OBJDIR)
BUILD_EXECDIR_BOOT = $(BUILD_EXECDIR)

#### Boot step 1

EXEC_SRCS_BOOT1 = cast2casm-boot1.cpp
EXEC_OBJS_BOOT1 = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS_BOOT1))
EXECS_BOOT1 = $(patsubst %.cpp, $(BUILD_EXECDIR_BOOT)/%$(EXE), $(EXEC_SRCS_BOOT1))

GENERATED_BOOT1_OBJS += $(EXEC_OBJS_BOOT1)
GENERATED_BOOT1_EXECS += $(EXECS_BOOT1)

#### Boot step 2

EXEC_SRCS_BOOT2 = cast2casm-boot2.cpp
EXEC_OBJS_BOOT2 = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS_BOOT2))
EXECS_BOOT2 = $(patsubst %.cpp, $(BUILD_EXECDIR_BOOT)/%$(EXE), $(EXEC_SRCS_BOOT2))

GENERATED_BOOT2_OBJS += $(EXEC_OBJS_BOOT2)
GENERATED_BOOT2_EXECS += $(EXECS_BOOT2)

##### Build result

EXEC_SRCS_REST = \
	cast2casm.cpp \
	casm2cast.cpp \
	compress-int.cpp \
	decompress.cpp
EXEC_OBJS_REST = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS_REST))
EXECS_REST = $(patsubst %.cpp, $(BUILD_EXECDIR)/%$(EXE), $(EXEC_SRCS_REST))
EXECS = $(EXECS_BOOT1) $(EXECS_BOOT2) $(EXECS_REST)

EXEC_SRCS = $(EXEC_SRCS_BOOT1) $(EXEC_SRCS_BOOT2) $(EXEC_SRCS_REST)
EXEC_OBJS = $(patsubst %.cpp, $(EXEC_OBJDIR)/%.o, $(EXEC_SRCS))

GENERATED_REST_OBJS += $(EXEC_OBJS_REST)
GENERATED_REST_EXECS += $(EXECS_REST)

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

GENERATED_REST_OBJS += $(TEST_OBJS)
GENERATED_REST_EXECS += $(TEST_EXECS)

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

TEST_CASM_M_SRC_FILES = $(patsubst %.wast, $(TEST_0XD_SRCDIR)/%.wasm-w, \
                        $(TEST_WASM_SRCS))

# The following groups are for different types of tests on decompress.
TEST_WASM_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm, \
                       $(TEST_WASM_SRCS))

TEST_WASM_M_GEN_FILES = $(patsubst %.wast, $(TEST_0XD_GENDIR)/%.wasm-w, \
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
	Wasm0xd.cast \
	ExprRedirects.cast \
	TableEx.cast

TEST_CASM_SRC_FILES = $(patsubst %.cast, $(TEST_SRCS_DIR)/%.cast, \
			$(TEST_CASM_SRCS))

TEST_CASM_CASM_FILES = $(patsubst %.cast, $(TEST_SRCS_DIR)/%.casm, \
			$(TEST_CASM_SRCS))

TEST_CASM_CASM_M_FILES = $(patsubst %.cast, $(TEST_SRCS_DIR)/%.casm-m, \
			  $(TEST_CASM_SRCS))

TEST_CASM_CAST_OUT_FILES = $(patsubst %.cast, $(TEST_SRCS_DIR)/%.cast-out, \
			  $(TEST_CASM_SRCS))

TEST_CASM_LITUSE_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-nolituse, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOLIT_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-nolits, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOACT_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-noacts, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOSYMACT_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-nosymacts, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOLITSYM_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-nolitsyms, \
				$(TEST_CASM_SRCS))


TEST_CASM_NOLITACT_FILES = $(patsubst %.cast, \
				$(TEST_SRCS_DIR)/%.cast-nolitacts, \
				$(TEST_CASM_SRCS))

TEST_CASM_GEN_FILES = $(patsubst %.cast, $(TEST_0XD_GENDIR)/%.casm, \
	 	  	$(TEST_CASM_SRCS))

TEST_WASM_M_GEN_FILES = $(patsubst %.cast, $(TEST_0XD_GENDIR)/%.casm-m, \
	 	  	$(TEST_CASM_SRCS))

TEST_CASM_OUT_GEN_FILES = $(patsubst %.cast, $(TEST_0XD_GENDIR)/%.cast-out, \
	 	  	$(TEST_CASM_SRCS))

TEST_CASM_LITUSE_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-nolituse, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOLIT_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-nolits, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOACT_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-noacts, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOSYMACT_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-nosymacts, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOLITSYM_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-nolitsyms, \
				$(TEST_CASM_SRCS))

TEST_CASM_NOLITACT_GEN_FILES = $(patsubst %.cast, \
				$(TEST_0XD_GENDIR)/%.cast-nolitacts, \
				$(TEST_CASM_SRCS))


###### Libraries for each compilation step ######

LIBS_BOOT1 = $(BINARY_LIB) \
	$(SEXP_LIB) $(PARSER_LIB) \
	$(BINARY_LIB) $(STRM_LIB) $(UTILS_LIB)

LIBS_BOOT2 = $(BINARY_LIB) $(INTERP_LIB_BASE) \
	$(SEXP_LIB) $(CASM_LIB_BASE) $(PARSER_LIB) \
	$(INTERP_LIB_BASE) $(ALG_BOOT2_LIB) $(BINARY_LIB) \
	$(STRM_LIB) $(UTILS_LIB) $(ALG_BOOT2_LIB)

LIBS = $(INTCOMP_LIB) $(BINARY_LIB) $(INTERP_LIB) $(SEXP_LIB) $(CASM_LIB) $(PARSER_LIB) \
       $(STRM_LIB) $(INTERP_LIB) $(BINARY_LIB) \
       $(ALG_LIB) $(SEXP_LIB) $(INTERP_LIB) $(BINARY_LIB) $(ALG_LIB) \
       $(CASM_LIB) $(STRM_LIB) $(UTILS_LIB) $(PARSER_LIB)

##### Track additional important variable definitions not in Makefile.common

CCACHE := `command -v ccache`
CPP_COMPILER := CCACHE_CPP2=yes $(CCACHE) $(CXX)

# Note: On WIN32 replace -fPIC with -D_GNU_SOURCE
# Note: g++ on Travis doesn't support -std=gnu++11
CXXFLAGS := $(TARGET_CXXFLAGS) $(PLATFORM_CXXFLAGS) \
            -Wall -Wextra -O2 -g -pedantic -MP -MD \
	    -Werror -Wno-unused-parameter -fno-omit-frame-pointer -fPIC \
	    -Isrc -I$(SRC_GENDIR)

ifneq ($(RELEASE), 0)
  CXXFLAGS += -DNDEBUG
endif

ifdef MAKE_PAGE_SIZE
  CXXFLAGS += -DWASM_DECODE_PAGE_SIZE=$(PAGE_SIZE)
endif

###### Combine all generated sources.

GENERATED = $(GENERATED_COPY_SOURCES)\
	$(GENERATED_BOOT1_SOURCES) \
	$(GENERATED_BOOT2_SOURCES)

###### Default Rule ######

ifeq ($(UPDATE), 0)
  default: build-all
else
  default: update-all
endif

.PHONY: default

all:
	$(MAKE) gen
	$(MAKE) build-all
	$(MAKE) test

build-all: $(LIBS) $(EXECS) $(TEST_EXECS)

###### Build submodules ######

wabt-submodule:
	cd $(WABT_DIR); git submodule update --init; make

.PHONY: wabt-submodule

###### Cleaning Rules #######

clean: clean-wabt
	rm -rf $(BUILDDIR) 

.PHONY: clean

clean-all: clean-wabt
	rm -rf $(BUILDBASEDIR)

.PHONY: clean-all

clean-wabt:
	cd $(WABT_DIR); make clean

.PHONY: clean-wabt

clean-gen:
	rm -rf $(BUILDDIR_BOOT) $(GENDIR)

###### Source Generation Rules #######

# TODO(karlschimpf): Don't know why, but bison/lex files need to be generated
# twice before dependencies don't cause a rebuild.

gen:
	$(MAKE) GENSRCS=1 gen-copy-sources
	$(MAKE) GENSRCS=2 gen-parse-sources
	$(MAKE) gen-boot1-execs
	$(MAKE) GENSRCS=3 gen-boot1-sources
	$(MAKE) gen-boot2-execs
	$(MAKE) GENSRCS=4 gen-boot2-sources
	@echo "** Gen step succeeded **"

.PHONY: gen

gen-copy-sources: $(GENERATED_COPY_SOURCES)

gen-parse-sources: $(GENERATED_PARSE_SOURCES)

gen-boot1-execs: $(GENERATED_BOOT1_EXECS)

gen-boot1-sources: $(GENERATED_BOOT1_SOURCES)

gen-boot2-objs: $(GENERATED_BOOT2_OBJS)

gen-boot2-libs: $(GENERATED_BOOT2_LIBS)

gen-boot2-execs: $(GENERATED_BOOT2_EXECS)

gen-boot2-sources: $(GENERATED_BOOT2_SOURCES)

$(GENERATED_BOOT1_LIBS): $(GENERATED_BOOT1_OBJS)

$(GENERATED_BOOT1_EXECS): $(GENERATED_BOOT1_LIBS)

$(GENERATED_BOOT2_OBJS): $(GENERATED_BOOT1_EXECS)

$(GENERATED_BOOT2_LIBS): $(GENERATED_BOOT2_OBJS)

$(GENERATED_BOOT2_EXECS): $(GENERATED_BOOT2_LIBS)

$(GENERATED_REST_LIBS): $(GENERATED_REST_OBJS)

$(GENERATED_REST_EXECS): $(GENERATED_REST_LIBS)

genvars:
	@echo "----------------------------------"
	@echo "Generartion build dependencies:"
	@echo ""
	@echo "COPY_SOURCES: $(GENERATED_COPY_SOURCES)"
	@echo ""
	@echo "PARSE_SOURCES: $(GENERATED_PARSE_SOURCES)"
	@echo ""
	@echo "BOOT1_OBJS: $(GENERATED_BOOT1_OBJS)"
	@echo ""
	@echo "BOOT1_LIBS: $(GENERATED_BOOT1_LIBS)"
	@echo ""
	@echo "BOOT1_EXECS: $(GENERATED_BOOT1_EXECS)"
	@echo ""
	@echo "BOOT1_SOURCES: $(GENERATED_BOOT1_SOURCES)"
	@echo ""
	@echo "BOOT2_OBJS: $(GENERATED_BOOT2_OBJS)"
	@echo ""
	@echo "BOOT2_LIBS: $(GENERATED_BOOT2_LIBS)"
	@echo ""
	@echo "BOOT2_EXECS: $(GENERATED_BOOT2_EXECS)"
	@echo ""
	@echo "BOOT2_SOURCES: $(GENERATED_BOOT2_SOURCES)"
	@echo ""
	@echo "REST_OBJS: $(GENERATED_REST_OBJS)"
	@echo ""
	@echo "REST_LIBS: $(GENERATED_REST_LIBS)"
	@echo ""
	@echo "REST_EXECS: $(GENERATED_REST_EXECS)"
	@echo ""
	@echo "----------------------------------"

###### Compiliing algorithm sources ######

$(ALG_GENDIR):
	mkdir -p $@

$(ALG_OBJDIR):
	mkdir -p $@

$(ALG_CAST_SRCS): | $(ALG_GENDIR)
$(ALG_CAST_LIT_SRCS): | $(ALG_GENDIR)

ifeq ($(GENSRCS), 1)
  $(ALG_CAST_SRCS): $(ALG_GENDIR)/%.cast: $(ALG_SRCDIR)/%.cast \
			$(ALG_CAST_LIT_SRCS)
	cp $< $@

  $(ALG_CAST_LIT_SRCS): $(ALG_GENDIR)/%.cast: $(ALG_SRCDIR)/%.cast
	cp $< $@
endif

$(ALG_OBJS): | $(ALG_OBJDIR)

-include $(foreach dep,$(ALG_OBJS:.o=.d), $(dep))

$(ALG_OBJS): $(ALG_OBJDIR)/%.o: $(ALG_GENDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(ALG_BOOT2_LIB): $(ALG_BOOT2_OBJS)
	ar -rs $@ $(ALG_BOOT2_OBJS)
	ranlib $@

$(ALG_LIB): $(ALG_OBJS)
	ar -rs $@ $(ALG_OBJS)
	ranlib $@

ifeq ($(GENSRCS), 3)

  # TODO(karlschimpf) Strip symbolic actions to save space. Note: This
  # requires a separate implementation file for enumerations.

  $(ALG_BOOT1_LIT_H_SRCS): $(ALG_GENDIR)/%.h: $(ALG_GENDIR)/%.cast \
		$(BUILD_EXECDIR_BOOT)/cast2casm-boot1
	@echo "CASE 1 $< $@"
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot1 \
		-a $< $< -o $@ --header --enum \
		--name $(call alg_name, $(patsubst %-lits.cast, %.cast, $<))

  $(ALG_BOOT1_LIT_CPP_SRCS): $(ALG_GENDIR)/%.cpp: $(ALG_GENDIR)/%.cast \
		$(BUILD_EXECDIR_BOOT)/cast2casm-boot1
	@echo "CASE 4 $< $@"
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot1 \
		-a $< $< -o $@ --enum \
		--name $(call alg_name, $(patsubst %-lits.cast, %.cast, $<))

  $(ALG_BOOT1_H_SRCS): $(ALG_GENDIR)/%Boot.h: $(ALG_GENDIR)/%Boot.cast \
		$(ALG_GENDIR)/%-lits.cast $(BUILD_EXECDIR_BOOT)/cast2casm-boot1
	@echo "CASE 2 $< $@"
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot1 \
		-a $(patsubst %Boot.cast, %-lits.cast, $<) -a $< \
		$< -o $@ --header --strip-literals --function \
		--name $(call alg_name, $<)

  $(ALG_BOOT1_CPP_SRCS): $(ALG_GENDIR)/%Boot.cpp: $(ALG_GENDIR)/%Boot.cast \
		$(ALG_GENDIR)/%-lits.cast $(BUILD_EXECDIR_BOOT)/cast2casm-boot1
	@echo "CASE 3 $< $@"
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot1 \
		-a $(patsubst %Boot.cast, %-lits.cast, $<) -a $< \
		$< -o $@ --strip-literals --function \
		--name $(call alg_name, $<)

endif

ifeq ($(GENSRCS), 4)

  # TODO(karlschimpf) Strip symbolic actions to save space. Note: This
  # requires a separate implementation file for enumerations.

  $(ALG_BOOT2_H_SRCS): $(ALG_GENDIR)/%.h: $(ALG_GENDIR)/%.cast \
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot2
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot2 \
		$< -o $@ --header --strip-literal-uses \
		-a $(ALG_GENDIR_ALG) --enum --function \
		--name $(call alg_name, $<) \
		$(if $(findstring casm0x0, $<), , --strip-actions)

  $(ALG_BOOT2_CPP_SRCS): $(ALG_GENDIR)/%.cpp: $(ALG_GENDIR)/%.cast \
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot2
	$(BUILD_EXECDIR_BOOT)/cast2casm-boot2  \
		$< -o $@ --strip-literal-uses --array \
		-a $(ALG_GENDIR_ALG) --enum --function \
		--name $(call alg_name, $<) \
		$(if $(findstring casm0x0, $<), --boot, --strip-actions)

endif

###### Compiliing binary Sources ######

$(BINARY_OBJS): | $(BINARY_OBJDIR)

$(BINARY_OBJDIR):
	mkdir -p $@


-include $(foreach dep,$(BINARY_SRCS:.cpp=.d),$(BINARY_OBJDIR)/$(dep))

$(BINARY_OBJS): $(BINARY_OBJDIR)/%.o: $(BINARY_DIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(BINARY_LIB): $(BINARY_OBJS)
	ar -rs $@ $(BINARY_OBJS)
	ranlib $@

###### Compiliing top-level Sources ######

$(UTILS_OBJS): | $(UTILS_OBJDIR)

$(UTILS_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(UTILS_SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(UTILS_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(UTILS_LIB): $(UTILS_OBJS)
	ar -rs $@ $(UTILS_OBJS)
	ranlib $@

###### Compiling s-expression interpeter sources ######

$(INTERP_OBJS): | $(INTERP_OBJDIR)

$(INTERP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(INTERP_SRCS:.cpp=.d),$(INTERP_OBJDIR)/$(dep))

$(INTERP_OBJS): $(INTERP_OBJDIR)/%.o: $(INTERP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(INTERP_LIB_BASE): $(INTERP_OBJS_BASE)
	ar -rs $@ $(INTERP_OBJS_BASE)
	ranlib $@

$(INTERP_LIB_C): $(INTERP_OBJS_C)
	ar -rs $@ $(INTERP_OBJS_C)
	ranlib $@

###### Compiling the integer compressor sources ######

$(INTCOMP_OBJS): | $(INTCOMP_OBJDIR)

$(INTCOMP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(INTCOMP_SRCS:.cpp=.d),$(INTCOMP_OBJDIR)/$(dep))

$(INTCOMP_OBJS): $(INTCOMP_OBJDIR)/%.o: $(INTCOMP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


$(INTCOMP_LIB): $(INTCOMP_OBJS)
	ar -rs $@ $(INTCOMP_OBJS)
	ranlib $@

###### Compiliing Sexp Sources ######

$(SEXP_OBJS): | $(SEXP_OBJDIR)

$(SEXP_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(SEXP_SRCS:.cpp=.d),$(SEXP_OBJDIR)/$(dep))

$(SEXP_OBJS): $(SEXP_OBJDIR)/%.o: $(SEXP_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(SEXP_LIB): $(SEXP_OBJS)
	ar -rs $@ $(SEXP_OBJS)
	ranlib $@

###### Compiliing casm ources ######

$(CASM_OBJS): | $(CASM_OBJDIR)

$(CASM_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(CASM_SRCS:.cpp=.d),$(CASM_OBJDIR)/$(dep))

$(CASM_OBJS): $(CASM_OBJDIR)/%.o: $(CASM_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@


$(CASM_LIB_BIN): $(CASM_OBJS_BIN)
	ar -rs $@ $(CASM_OBJS_BIN)
	ranlib $@

$(CASM_LIB_BASE): $(CASM_OBJS_BASE)
	ar -rs $@ $(CASM_OBJS_BASE)
	ranlib $@

###### Compiling stream sources ######

$(STRM_OBJS): | $(STRM_OBJDIR)

$(STRM_OBJDIR):
	mkdir -p $@

-include $(foreach dep,$(STRM_SRCS:.cpp=.d),$(STRM_OBJDIR)/$(dep))

$(STRM_OBJS): $(STRM_OBJDIR)/%.o: $(STRM_SRCDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(STRM_LIB): $(STRM_OBJS)
	ar -rs $@ $(STRM_OBJS)
	ranlib $@

###### Compiling Filter Parser #######

$(PARSER_GENDIR):
	mkdir -p $@

$(PARSER_OBJDIR):
	mkdir -p $@

$(PARSER_OBJS): | $(PARSER_OBJDIR)

ifeq ($(GENSRCS), 1)

  $(PARSER_GENDIR)/Lexer.lex: $(PARSER_DIR)/Lexer.lex $(PARSER_GENDIR)
	cp $< $@

  $(PARSER_GENDIR)/Parser.ypp: $(PARSER_DIR)/Parser.ypp $(PARSER_GENDIR)
	cp $< $@

endif

ifeq ($(GENSRCS), 2)

  $(PARSER_GENDIR)/Lexer.cpp: $(PARSER_GENDIR)/Lexer.lex $(PARSER_GENDIR)
	cd $(PARSER_GENDIR); lex -o Lexer.cpp Lexer.lex

  $(PARSER_GENDIR)/Parser.tab.cpp: $(PARSER_GENDIR)/Parser.ypp $(PARSER_GENDIR)
	cd $(PARSER_GENDIR); bison -d -r all Parser.ypp
	cd $(PARSER_GENDIR); touch $(PARSER_PARSE_OTHER_GENSRCS)

endif

-include $(foreach dep,$(PARSER_OBJS:.o=.d), $(dep))

$(PARSER_REST_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_DIR)/%.cpp $(PARSER_GEN_SRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(PARSER_GEN_OBJS): $(PARSER_OBJDIR)/%.o: $(PARSER_GENDIR)/%.cpp $(PARSER_GEN_SRCS)
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(PARSER_LIB): $(PARSER_OBJS)
	ar -rs $@ $(PARSER_OBJS)
	ranlib $@

###### Building libraries ######

$(LIBS_BOOT1): | $(LIBDIR)
$(LIBS_BOOT2): | $(LIBDIR)
$(LIBS): | $(LIBDIR)

$(LIBDIR):
	mkdir -p $@

###### Compiling executables ######

$(EXEC_OBJDIR):
	mkdir -p $@

$(BUILD_EXECDIR):
	mkdir -p $@

$(EXEC_OBJS): | $(EXEC_OBJDIR)

$(EXECS): | $(BUILD_EXECDIR)

#### Boot step 1

-include $(foreach dep,$(EXEC_SRCS_BOOT1:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS_BOOT1): $(EXEC_OBJDIR)/%.o: $(EXECDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(EXECS_BOOT1): $(BUILD_EXECDIR_BOOT)/%$(EXE): $(EXEC_OBJDIR)/%.o \
	$(LIBS_BOOT1)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS_BOOT1) -o $@

#### Boot step 2

-include $(foreach dep,$(EXEC_SRCS_BOOT2:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS_BOOT2): $(EXEC_OBJDIR)/%.o: $(EXECDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(EXECS_BOOT2): $(BUILD_EXECDIR_BOOT)/%$(EXE): $(EXEC_OBJDIR)/%.o \
	$(LIBS_BOOT2)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS_BOOT2) -o $@

#### build rest.

-include $(foreach dep,$(EXEC_SRCS:.cpp=.d),$(EXEC_OBJDIR)/$(dep))

$(EXEC_OBJS_REST): $(EXEC_OBJDIR)/%.o: $(EXECDIR)/%.cpp
	$(CPP_COMPILER) -c $(CXXFLAGS) $< -o $@

$(EXECS_REST): $(BUILD_EXECDIR)/%$(EXE): $(EXEC_OBJDIR)/%.o $(LIBS)
	$(CPP_COMPILER) $(CXXFLAGS) $< $(LIBS) -o $@

###### Compiling Test Executables #######

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

test: build-all test-parser test-raw-streams test-byte-queues \
	test-huffman test-decompress test-casm2cast test-cast2casm \
	test-casm-cast test-compress 
	@echo "*** all tests passed ***"

.PHONY: test

test-all:
	@echo "*** testing release version ***"
	$(MAKE) $(MAKE_PAGE_SIZE) DEBUG=0 RELEASE=1 test
	@echo "*** testing debug version ***"
	$(MAKE) $(MAKE_PAGE_SIZE)  DEBUG=1 RELEASE=0 test
	@echo "*** all tests passed on both debug and release builds ***"

.PHONY: test-all

presubmit:
	$(MAKE) clean-all
	$(MAKE) gen
	$(MAKE) test-all CXX=clang++ PAGE_SIZE=2
	$(MAKE) $(MAKE_PAGE_SIZE) clean
	$(MAKE) $(MAKE_PAGE_SIZE) test-all CXX=g++
	$(MAKE) $(MAKE_PAGE_SIZE) clean
	$(MAKE) $(MAKE_PAGE_SIZE) test-all CXX=clang++
	@echo "*** Presubmit tests passed ***"

.PHONY: presubmit

test-huffman: $(TEST_EXECDIR)/TestHuffman
	$< | diff - $(TEST_SRCS_DIR)/TestHuffman.out
	@echo "*** Huffman encoding tests passed ***"

.PHONY: test-huffman

test-decompress: \
	$(TEST_WASM_GEN_FILES) \
	$(TEST_WASM_M_GEN_FILES) \
	$(TEST_WASM_CAPI_GEN_FILES) \
	$(TEST_WASM_WS_GEN_FILES) \
	$(TEST_WASM_SW_GEN_FILES)
	@echo "*** decompress 0xD tests passed ***"

.PHONY: test-decompress

test-compress: $(TEST_WASM_COMP_FILES)
	@echo "*** compress 0xD tests passed ***"

.PHONY: test-compress

$(TEST_CASM_GEN_FILES): $(TEST_0XD_GENDIR)/%.casm: $(TEST_SRCS_DIR)/%.cast \
		$(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< | \
		cmp - $(patsubst %.cast, %.casm, $<)

.PHONY: $(TEST_CASM_GEN_FILES)

$(TEST_WASM_M_GEN_FILES): $(TEST_0XD_GENDIR)/%.casm-m: $(TEST_SRCS_DIR)/%.cast \
		$(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm -m $< | \
		cmp - $(patsubst %.cast, %.casm-m, $<)

.PHONY: $(TEST_WASM_M_GEN_FILES)

$(TEST_CASM_OUT_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-out: $(TEST_SRCS_DIR)/%.casm \
		$(TEST_SRCS_DIR)/%.casm $(BUILD_EXECDIR)/casm2cast
	$(BUILD_EXECDIR)/casm2cast $< | \
		cmp - $(patsubst %.casm, %.cast-out, $<)
	$(BUILD_EXECDIR)/casm2cast \
		$(patsubst %.casm, %.casm-m, $<) | \
		cmp - $(patsubst %.casm, %.cast-out, $<)

.PHONY: $(TEST_CASM_OUT_GEN_FILES)

$(TEST_CASM_LITUSE_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-nolituse: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-literal-uses --display=stripped \
		| cmp - $(patsubst %.cast, %.cast-nolituse, $<)

.PHONY: $(TEST_CASM_LITUSE_GEN_FILES)

$(TEST_CASM_NOLIT_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-nolits: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-literals --display=stripped \
		| cmp - $(patsubst %.cast, %.cast-nolits, $<)

.PHONY: $(TEST_CASM_NOLIT_GEN_FILES)

$(TEST_CASM_NOACT_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-noacts: \
		 $(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-actions --display=stripped \
		| cmp - $(patsubst %.cast, %.cast-noacts, $<)

.PHONY: $(TEST_CASM_NOACT_GEN_FILES)

$(TEST_CASM_NOSYMACT_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-nosymacts: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-symbolic-actions \
		--display=stripped \
		| cmp - $(patsubst %.cast, %.cast-nosymacts, $<)

.PHONY: $(TEST_CASM_NOSYMACT_GEN_FILES)

$(TEST_CASM_NOLITSYM_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-nolitsyms: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-symbolic-actions \
		--strip-literals --display=stripped \
		| cmp - $(patsubst %.cast, %.cast-nolitsyms, $<)

.PHONY: $(TEST_CASM_NOLITSYM_GEN_FILES)


$(TEST_CASM_NOLITACT_GEN_FILES): $(TEST_0XD_GENDIR)/%.cast-nolitacts: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/cast2casm
	$(BUILD_EXECDIR)/cast2casm $< --strip-actions \
		--strip-literals --display=stripped \
		| cmp - $(patsubst %.cast, %.cast-nolitacts, $<)

.PHONY: $(TEST_CASM_NOLITACT_GEN_FILES)

# Note: Currently only tests that code executes (without errors).
$(TEST_WASM_COMP_FILES): $(TEST_0XD_GENDIR)/%.wasm-comp: $(TEST_0XD_SRCDIR)/%.wasm \
		$(BUILD_EXECDIR)/compress-int $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/compress-int --min-count 2 --min-weight 5 $< \
	| $(BUILD_EXECDIR)/decompress - | cmp - $<
	$(BUILD_EXECDIR)/compress-int --Huffman --min-count 2 --min-weight 5 $< \
	| $(BUILD_EXECDIR)/decompress - | cmp - $<

.PHONY: $(TEST_WASM_COMP_FILES)

$(TEST_WASM_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm: $(TEST_0XD_SRCDIR)/%.wasm \
		$(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress $< | cmp - $<

.PHONY: $(TEST_WASM_GEN_FILES)

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
		$(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress -p -d $(ALG_SRCDIR)/wasm0xd.cast \
		 $< | cmp - $<

.PHOHY: $(TEST_WASM_WPD_GEN_FILES)


$(TEST_WASM_CAPI_GEN_FILES): $(TEST_0XD_GENDIR)/%.wasm-capi: \
		$(TEST_0XD_SRCDIR)/%.wasm-w $(BUILD_EXECDIR)/decompress
	$(BUILD_EXECDIR)/decompress --c-api $< | cmp - $<

.PHOHY: $(TEST_WASM_WPD_GEN_FILES)

test-cast2casm: $(TEST_CASM_GEN_FILES) $(TEST_WASM_M_GEN_FILES) \
		$(TEST_CASM_LITUSE_GEN_FILES) \
		$(TEST_CASM_NOLIT_GEN_FILES) \
		$(TEST_CASM_NOACT_GEN_FILES) \
		$(TEST_CASM_NOSYMACT_GEN_FILES) \
		$(TEST_CASM_NOLITSYM_GEN_FILES) \
		$(TEST_CASM_NOLITACT_GEN_FILES) 
	@echo "*** cast2casm tests passed ***"

.PHONY: test-cast2cast

test-casm2cast: $(BUILD_EXECDIR)/casm2cast $(TEST_CASM_OUT_GEN_FILES)
	$< $(TEST_DEFAULT_CASM) | diff - $(TEST_DEFAULT_CAST_OUT)
	$< $(TEST_DEFAULT_CASM_M) | diff - $(TEST_DEFAULT_CAST_OUT)
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
	$< -w $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST_OUT)
	$< --expect-fail $(TEST_SRCS_DIR)/MismatchedParens.cast 2>&1 | \
		diff - $(TEST_SRCS_DIR)/MismatchedParens.cast-out
	$< -w $(TEST_SRCS_DIR)/ExprRedirects.cast | \
		diff - $(TEST_SRCS_DIR)/ExprRedirects.cast-out
	$< -w $(TEST_SRCS_DIR)/BinaryFormat.cast | \
		diff - $(TEST_SRCS_DIR)/BinaryFormat.cast-out
	@echo "*** parser tests passed ***"

.PHONY: test-parser

test-raw-streams: $(TEST_EXECDIR)/TestRawStreams
	$< -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	@echo "*** test raw streams passed ***"

.PHONY: test-raw-streams

test-byte-queues: $(TEST_EXECDIR)/TestByteQueues
	$< -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 2 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 3 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 4 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 5 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 6 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 7 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 13 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 119 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 2323 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
	$< -c 3231 -i $(TEST_DEFAULT_CAST) | diff - $(TEST_DEFAULT_CAST)
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

###### Updating tests sources ######

ifneq ($(UPDATE), 0)

update-all: wabt-submodule build-all
	$(TEST_WASM_SRC_FILES) \
	$(TEST_CASM_M_SRC_FILES) \
	$(TEST_CASM_CASM_FILES) \
	$(TEST_CASM_CASM_M_FILES) \
	$(TEST_CASM_CAST_OUT_FILES) \
	$(TEST_CASM_LITUSE_FILES)

$(TEST_WASM_SRC_FILES): $(TEST_0XD_SRCDIR)/%.wasm: $(WABT_WAST_DIR)/%.wast \
		wabt-submodule
	$(WABT_DIR)/out/wast2wasm -o $@ $<

$(TEST_CASM_M_SRC_FILES): $(TEST_0XD_SRCDIR)/%.wasm-w: $(WABT_WAST_DIR)/%.wast \
		wabt-submodule
	$(WABT_DIR)/out/wast2wasm --no-canonicalize-leb128s -o $@ $<

$(TEST_CASM_CASM_FILES): $(TEST_SRCS_DIR)/%.casm: $(TEST_SRCS_DIR)/%.cast \
		$(BUILD_EXECDIR)/cast2casm
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< -o $@

$(TEST_CASM_CASM_M_FILES): $(TEST_SRCS_DIR)/%.casm-m: $(TEST_SRCS_DIR)/%.cast \
		$(BUILD_EXECDIR)/cast2casm
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm -m $< -o $@

$(TEST_CASM_CAST_OUT_FILES): $(TEST_SRCS_DIR)/%.cast-out: \
		$(TEST_SRCS_DIR)/%.casm $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/casm2cast $< -o $@

$(TEST_CASM_LITUSE_FILES): $(TEST_SRCS_DIR)/%.cast-nolituse: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-literal-uses \
		--display=stripped > $@

$(TEST_CASM_NOLIT_FILES): $(TEST_SRCS_DIR)/%.cast-nolits: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-literals \
		--display=stripped > $@

$(TEST_CASM_NOACT_FILES): $(TEST_SRCS_DIR)/%.cast-noacts: $(TEST_SRCS_DIR)/%.cast \
		$(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-actions \
		--display=stripped > $@

$(TEST_CASM_NOSYMACT_FILES): $(TEST_SRCS_DIR)/%.cast-nosymacts: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-symbolic-actions \
		--display=stripped > $@

$(TEST_CASM_NOLITSYM_FILES): $(TEST_SRCS_DIR)/%.cast-nolitsyms: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-symbolic-actions \
		--strip-literals --display=stripped > $@


$(TEST_CASM_NOLITACT_FILES): $(TEST_SRCS_DIR)/%.cast-nolitacts: \
		$(TEST_SRCS_DIR)/%.cast $(BUILD_EXECDIR)/casm2cast
	rm -rf $@; $(BUILD_EXECDIR)/cast2casm $< --strip-actions \
		--strip-literals --display=stripped > $@

endif

