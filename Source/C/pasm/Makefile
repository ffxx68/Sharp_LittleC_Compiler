CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c99

ifeq ($(OS),Windows_NT)
TARGET := pasm.exe
else
TARGET := pasm
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): pasm.c pasm_constants.h
	$(CC) $(CFLAGS) -o $@ pasm.c

clean:
	rm -f pasm pasm.exe
