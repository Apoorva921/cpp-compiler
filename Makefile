CXX ?= g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2 -Isrc

SRCS = src/lexer/token.cpp \
       src/lexer/lexer.cpp \
       src/ast/ast.cpp \
       src/parser/parser.cpp \
       src/semantics/type_checker.cpp \
       src/codegen/codegen.cpp \
       src/main.cpp

ifeq ($(OS),Windows_NT)
    TARGET = mcxx.exe
else
    TARGET = mcxx
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) *.exe *.s a.out

.PHONY: all clean
