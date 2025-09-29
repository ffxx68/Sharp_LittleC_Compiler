# Makefile for lcpp C version
# Compiler for the Little C preprocessor

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = lcpp
SOURCE = lcpp.c

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Build for Windows
windows: $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET).exe $(SOURCE)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET).exe

# Install to main directory
install: $(TARGET)
	cp $(TARGET) ../../../$(TARGET)
	cp $(TARGET).exe ../../../$(TARGET).exe 2>/dev/null || true

# Test with existing test files
test: $(TARGET)
	./$(TARGET) test_input.c test_output.c
	@echo "Test completed. Check test_output.c"

.PHONY: all windows clean install test
