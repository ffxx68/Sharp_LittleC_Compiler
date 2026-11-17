# lcc2 Library Reference

This is a reference to the library subset supplied with `lcc2`; it is not a
full hosted-C library. For language support and compiler usage, see the
[language manual](lcc2_manual.md). The [project README](../Source/C/lcc2/README.md)
documents builds and runtime tests.

## Using library headers

Headers are in `Source\C\lcc2\include`. Preprocess separately, then compile
and assemble from the repository root in PowerShell:

```powershell
gcc -E -P -nostdinc -I Source\C\lcc2\include .\program.c -o .\program.i
.\Source\C\lcc2\lcc2.exe --target PC1403 .\program.i -o .\program.asm
.\pasm.exe .\program.asm .\program.bin
```

Omit `--target` for portable library functions. Target options are `PC1350`,
`PC1401`, and `PC1403`; they select only the matching device implementation.
No target runtime is appended when no target is selected.

The driver appends a module only when generated code calls its function, and
includes each module once. A declaration by itself adds no code. A
user-defined implementation takes precedence over the supplied one. The
`puts` adapter also requires `putchar`; it uses a user-defined implementation
if present. There is no general external-object linker.

## Supplied headers

### `stdint.h`: fixed-width types

| Name | Definition |
|---|---|
| `uint8_t` | `unsigned char` |
| `uint16_t` | `unsigned int` |

These are typedefs only. No other `stdint.h` types or macros are supplied.

### `string.h`: copying and concatenation

```c
char *strcpy(char *destination, const char *source);
char *strcat(char *destination, const char *source);
```

Both functions copy through the source NUL and return the original
destination pointer. The caller must provide valid NUL-terminated strings,
sufficient destination space, and nonoverlapping source and destination.
There is no allocation, capacity check, or truncation. Each function is an
independent, device-independent runtime module.

String literals and array-to-pointer decay are unsupported. Use initialized
character arrays and pass an element address, for example
`strcpy(&destination[0], &source[0])`. The legacy root `string.h` has a
different, three-argument `strcpy`; it is not this API. Other standard string
functions are not supplied.

### `stdio.h`: minimal character I/O

```c
int getchar(void);
int putchar(int character);
int puts(const char *text);
```

`getchar` and `putchar` are target-specific; see the
[PC-1350](lcc2_libraries_pc1350.md),
[PC-1401](lcc2_libraries_pc1401.md), and
[PC-1403](lcc2_libraries_pc1403.md) references for availability and behavior.
These functions do not implement standard-C streams.

**`puts` is nonstandard:** it writes the string bytes through `putchar`, but
does not append a newline. It returns 0 on success and -1 (`0xFFFF`) on the
first output error. A successfully written prefix remains; an empty string
succeeds without calling `putchar`. It can be used without `--target` if the
program supplies `putchar`.

### `compat.h`: legacy typedefs

| Name | Definition |
|---|---|
| `byte` | `unsigned char` |
| `word` | `unsigned int` |

Typedefs only; this is a compatibility header, not a standard-C header.

### `int2str.h`: unsigned decimal conversion

```c
void int2str(unsigned int number, char *destination);
```

Converts 0..65535 to decimal ASCII with no sign or leading zeroes, followed
by NUL. The destination needs at least six writable bytes. The function is
device-independent, has no return value, and does not provide signed
conversion or capacity checking.

## Target references

| Target | Supplied device APIs |
|---|---|
| PC-1350 | Raw keyboard input, display on/off, and ROM-backed BCD/string operations. [Reference](lcc2_libraries_pc1350.md) |
| PC-1401 | Raw keyboard input and display on/off. [Reference](lcc2_libraries_pc1401.md) |
| PC-1403 | Raw keyboard input, character output, and LCD control/pixel operations. [Reference](lcc2_libraries_pc1403.md) |

The library implementations live in `Source\C\lcc2\runtime` and
`Source\C\lcc2\targets`. The compiler's target selection does not reserve
memory for code, data, or runtime workspaces; placement remains the
programmer's responsibility.
