CC = gcc
FLEX ?= flex
BISON ?= bison
CFLAGS ?= -Wall -Wextra -Werror -O2 -std=c99
BUILD_DIR := build
TARGET := lcc2.exe
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/ast.o $(BUILD_DIR)/lexer.o
TEST_TARGET := $(BUILD_DIR)/test_ast.exe
LEXER_TEST_TARGET := $(BUILD_DIR)/test_lexer.exe

.PHONY: all check-tools test clean

all: check-tools $(TARGET)

check-tools:
	@$(CC) --version >/dev/null
	@$(FLEX) --version >/dev/null
	@$(BISON) --version >/dev/null

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/lexer.c: lexer.l lexer_tokens.h | $(BUILD_DIR)
	$(FLEX) -o $@ lexer.l

$(BUILD_DIR)/lexer.o: $(BUILD_DIR)/lexer.c lexer_tokens.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

$(TEST_TARGET): tests/test_ast.c ast.c ast.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_ast.c ast.c

$(LEXER_TEST_TARGET): tests/test_lexer.c $(BUILD_DIR)/lexer.c lexer_tokens.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_lexer.c $(BUILD_DIR)/lexer.c

test: all $(TEST_TARGET) $(LEXER_TEST_TARGET)
	./$(TEST_TARGET)
	./$(LEXER_TEST_TARGET)
	./$(TARGET) --help
	./$(TARGET) --version

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
