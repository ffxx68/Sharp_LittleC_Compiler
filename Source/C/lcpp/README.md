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

### Using GCC:
```bash
gcc -Wall -Wextra -O2 -std=c99 -o lcpp lcpp.c
```

### Using the Makefile:
```bash
make          # Build lcpp
make windows  # Build lcpp.exe for Windows
make clean    # Clean build artifacts
make install  # Install to main directory
make test     # Run tests
```

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
