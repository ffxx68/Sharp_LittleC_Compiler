CC = gcc
FLEX ?= flex
BISON ?= bison
CFLAGS ?= -Wall -Wextra -Werror -O2 -std=c99
BUILD_DIR := build
TARGET := lcc2.exe
SOURCES := main.c ast.c
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/%.o)
TEST_TARGET := $(BUILD_DIR)/test_ast.exe

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

$(BUILD_DIR):
	mkdir -p $@

$(TEST_TARGET): tests/test_ast.c ast.c ast.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_ast.c ast.c

test: all $(TEST_TARGET)
	./$(TEST_TARGET)
	./$(TARGET) --help
	./$(TARGET) --version

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
