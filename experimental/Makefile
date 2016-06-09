# The following variables will likely need to be modified, depending on where
# and how you built LLVM & Clang. They can be overridden in a command-line
# invocation of make, like:
#
#   make CLANG_PATH=<path> ...

# # CLANG_PATH is the location of the clang compiler to use for building
# the host binaries.
CLANG_CPP = clang++

OBJDIR = build/objs
TESTDIR = build

SRCS = \
	BitStreamReader.cpp \
	ByteStreamReader.cpp \
	CircBuffer.cpp \
	DecodeDefs.cpp \
	FileReader.cpp \
	FileWriter.cpp \
	FilterDriver.cpp \
	filter_lexer.cpp \
	filter_parser.tab.cpp

OBJS=$(patsubst %.cpp, $(OBJDIR)/%.o, $(SRCS))

TEST_SRCS = \
	TestBitReader.cpp \
	TestByteReader.cpp \
	TestParsing.cpp

TESTS=$(patsubst %.cpp, $(TESTDIR)/%, $(TEST_SRCS))

$(info -----------------------------------------------)
$(info Using CLANG_PATH = $(CLANG_PATH))
#$(info Using OBJS = $(OBJS))
#$(info using TESTS = $(TESTS))
$(info -----------------------------------------------)

CCACHE := `command -v ccache`
CXX := CCACHE_CPP2=yes $(CCACHE) $(CLANG_CPP)

#? -fno-rtti -fno-exceptions
CXXFLAGS := -std=gnu++11 -Wall -Wextra \
  -O2 -g -pedantic -MP -MD -Werror -Isrc

all: gen objs tests

clean: clean-lexer clean-parser
	rm -rf build

clean-lexer:
	cd src; rm -f filter_lexer.cpp

clean-parser:
	cd src; rm -f filter_parser.output filter_parser.tab.cpp \
		filter_parser.tab.hpp location.hh position.hh stack.hh

gen: lexer parser

lexer: src/filter_lexer.cpp

src/filter_lexer.cpp: src/filter_lexer.lex
	cd src; lex -o filter_lexer.cpp filter_lexer.lex

parser: src/filter_parser.tab.cpp

src/filter_parser.tab.cpp: src/filter_parser.ypp
	cd src; bison -d -r all filter_parser.ypp

objs: $(OBJS)

tests: $(TESTS)

-include $(foreach dep,$(SRCS:.cpp=.d),$(OBJDIR)/$(dep))

$(OBJS): $(OBJDIR)/%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

-include $(foreach dep,$(SRCS:.cpp=.d),$(TESTDIR)/$(dep))

$(TESTS): $(TESTDIR)/%: test/%.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) -o $@

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

.PHONY: all clean objs tests gen clean-lexer clean-parser
.PHONY: lexer parser
