# LCPP - Little C Preprocessor (C Version)

This is a C migration of the original Pascal version of the Little C Preprocessor (lcpp) by Simon Lehmayr (2004).

## Features

The preprocessor supports the following directives:
- `#define` - Define macros/symbols
- `#include` - Include other files
- `#ifdef` - Conditional compilation
- `#endif` - End conditional block
- `#org` - Assembly origin directive (passed through)
- `#asm` / `#endasm` - Assembly blocks (passed through)
- `#nosave` - No save directive (passed through)

## Building

The build requires GCC and GNU Make. From this directory:

```sh
make
```

This builds `lcpp` on Linux. On Windows, run `make` for `lcpp`, or use the
Makefile's Windows target to produce `lcpp.exe`:

```bash
make windows
```

To build directly with GCC on either platform:

```bash
gcc -Wall -Wextra -O2 -std=c99 -o lcpp lcpp.c
```

`make clean` removes both executable names. `make test` runs the Makefile's
sample test; `make install` copies the default `lcpp` target to the parent
project directory.

## Usage

```bash
lcpp input1.c [input2.c ...] output.c
```

The preprocessor will:
1. Process all input files in order
2. Handle includes recursively
3. Expand defined macros
4. Handle conditional compilation
5. Write the processed output to the specified output file

## Changes from Pascal Version

- Migrated from Pascal to C99
- Added proper error handling for file operations
- Improved memory safety with bounds checking
- Added Windows-specific build target
- Enhanced Makefile with multiple targets
- Maintained full compatibility with original functionality

## Compatibility

This C version maintains full compatibility with the original Pascal version and produces identical output for the same input files.
