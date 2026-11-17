CC = gcc
FLEX ?= flex
BISON ?= bison
CFLAGS ?= -Wall -Wextra -Werror -O2 -std=c99
BUILD_DIR := build
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
else
UNAME_S := $(shell uname -s)
ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
EXEEXT := .exe
else
EXEEXT :=
endif
endif

TARGET := lcc2$(EXEEXT)
PASM := ../pasm/pasm$(EXEEXT)
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/ast.o $(BUILD_DIR)/symtab.o $(BUILD_DIR)/codegen.o $(BUILD_DIR)/peephole.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/lexer.o
TEST_TARGET := $(BUILD_DIR)/test_ast$(EXEEXT)
LEXER_TEST_TARGET := $(BUILD_DIR)/test_lexer$(EXEEXT)
PARSER_TEST_TARGET := $(BUILD_DIR)/test_parser$(EXEEXT)
CODEGEN_TEST_TARGET := $(BUILD_DIR)/test_codegen$(EXEEXT)
PEEPHOLE_TEST_TARGET := $(BUILD_DIR)/test_peephole$(EXEEXT)
SYMTAB_TEST_TARGET := $(BUILD_DIR)/test_symtab$(EXEEXT)

.PHONY: all check-tools test test-pasm clean

all: check-tools $(TARGET)

check-tools:
	@$(CC) --version >/dev/null
	@$(FLEX) --version >/dev/null
	@$(BISON) --version >/dev/null

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.c ast.h codegen.h parser.h symtab.h peephole.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/lexer.c: lexer.l lexer_tokens.h | $(BUILD_DIR)
	$(FLEX) -o $@ lexer.l

$(BUILD_DIR)/parser.c: parser.y parser.h ast.h lexer_tokens.h | $(BUILD_DIR)
	$(BISON) -Wall -Werror --defines=$(BUILD_DIR)/parser_tokens.h -o $@ parser.y

$(BUILD_DIR)/parser_tokens.h: $(BUILD_DIR)/parser.c
	@test -f $@ || $(BISON) -Wall -Werror --defines=$@ -o $(BUILD_DIR)/parser.c parser.y

$(BUILD_DIR)/parser.o: $(BUILD_DIR)/parser.c $(BUILD_DIR)/parser_tokens.h
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -c -o $@ $<

$(BUILD_DIR)/lexer.o: $(BUILD_DIR)/lexer.c $(BUILD_DIR)/parser_tokens.h lexer_tokens.h
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

$(TEST_TARGET): tests/test_ast.c ast.c ast.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_ast.c ast.c

$(LEXER_TEST_TARGET): tests/test_lexer.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c lexer_tokens.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -o $@ tests/test_lexer.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c ast.c

$(PARSER_TEST_TARGET): tests/test_parser.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c ast.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -o $@ tests/test_parser.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c ast.c

$(SYMTAB_TEST_TARGET): tests/test_symtab.c symtab.c symtab.h ast.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -o $@ tests/test_symtab.c symtab.c ast.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c

$(CODEGEN_TEST_TARGET): tests/test_codegen.c codegen.c codegen.h peephole.c peephole.h symtab.c symtab.h ast.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I. -o $@ tests/test_codegen.c codegen.c peephole.c symtab.c ast.c $(BUILD_DIR)/parser.c $(BUILD_DIR)/lexer.c

$(PEEPHOLE_TEST_TARGET): tests/test_peephole.c peephole.c peephole.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_peephole.c peephole.c

test-pasm:
	$(MAKE) -C ../pasm CC="$(CC)" TARGET="pasm$(EXEEXT)"

test: all test-pasm $(TEST_TARGET) $(LEXER_TEST_TARGET) $(PARSER_TEST_TARGET) $(SYMTAB_TEST_TARGET) $(CODEGEN_TEST_TARGET) $(PEEPHOLE_TEST_TARGET)
	./$(TEST_TARGET)
	./$(LEXER_TEST_TARGET)
	./$(PARSER_TEST_TARGET)
	./$(SYMTAB_TEST_TARGET)
	./$(CODEGEN_TEST_TARGET)
	./$(PEEPHOLE_TEST_TARGET)
	./$(TARGET) --help
	./$(TARGET) --version
	LCC2="$(abspath $(TARGET))" PASM="$(abspath $(PASM))" sh tests/test_cli.sh
	LCC2="$(abspath $(TARGET))" PASM="$(abspath $(PASM))" sh tests/test_strings.sh
	LCC2="$(abspath $(TARGET))" PASM="$(abspath $(PASM))" sh tests/test_libraries.sh
	LCC2="$(abspath $(TARGET))" PASM="$(abspath $(PASM))" sh tests/test_pc1350_rom.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
