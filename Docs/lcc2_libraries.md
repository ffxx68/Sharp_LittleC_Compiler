# lcc2 Library Reference

This document describes the library subset supplied with `lcc2`, not the
legacy Pascal compiler's root-level headers. Those legacy headers remain
unchanged. This is not a full hosted-C library.

See the [language manual](lcc2_manual.md) for compiler syntax, supported types,
the call ABI, and deployment. The [project README](../Source/C/lcc2/README.md)
contains repeatable compile/assembly and MAME runtime checks.

## Including headers and selecting implementations

Headers live in `Source\C\lcc2\include`. Preprocess source separately with a
preprocessor supporting standard include guards and include paths, for example
GCC. From the repository root in PowerShell:

```powershell
gcc -E -P -nostdinc -I Source\C\lcc2\include .\program.c -o .\program.preprocessed.c
.\Source\C\lcc2\lcc2.exe .\program.preprocessed.c -o .\program.asm
.\pasm.exe .\program.asm .\program.bin
```

Add `--target PC1403` to the `lcc2` invocation when using the supplied
device-dependent I/O implementations. Without a target, no device runtime is
appended; device-independent string routines remain available.

The command-line driver appends each external library module only when its
function is called in generated code, once per module. Header declarations
alone add nothing. A user-defined implementation does not select the supplied
module. Selection is based on emitted calls, not whole-program reachability.
The routines use the compiler's normal byte/word/pointer call ABI. There is
no general external-object linker.

Library implementations are outside codegen: device-independent SC61860
modules are in `Source\C\lcc2\runtime`, and device implementations are in
`Source\C\lcc2\targets\PC1403`. Standard API declarations and device-specific
behavior are documented separately below.

## Standard-library subsets

### `stdint.h`: fixed-width types

| Name | Definition |
|------|------------|
| `uint8_t` | `unsigned char`, 8 bits |
| `uint16_t` | `unsigned int`, 16 bits |

These are typedefs, with no runtime module. No other `stdint.h` types or
macros are currently supplied.

### `string.h`: copying and concatenation

Only these standard signatures are supplied:

```c
char *strcpy(char *destination, const char *source);
char *strcat(char *destination, const char *source);
```

`strcpy` copies the source including its NUL terminator. `strcat` appends the
source including its NUL at the destination's existing terminator. Both
return the original destination pointer.

The caller must provide valid NUL-terminated strings, sufficient destination
storage including the final NUL, and nonoverlapping source/destination regions.
There is no allocation, capacity check, or truncation. Empty strings and
nonzero byte values above 127 are supported. Source and destination may
independently be in internal RAM or XRAM, using the compiler's 16-bit pointers.

The implementations are separate `runtime\strcpy.asm` and `runtime\strcat.asm`
modules, available with or without `--target`. Using one never includes the
other. They have no device ROM calls, LCD/keyboard state, shared buffers, or
dependencies on each other.

String literals and array-to-pointer decay remain unsupported. Initialize
global character arrays with brace lists and pass `&array[0]`; automatic
arrays cannot be addressed. For example:

```c
#include "string.h"

__xram __at(0xE600) char destination[8];
__xram __at(0xE610) char source[3] = {65, 66, 0};

void main(void)
{
    strcpy(&destination[0], &source[0]);
    strcat(&destination[0], &source[0]);
}
```

The example leaves `ABAB` followed by NUL in `destination`. Reserve memory
for both code and data before execution, as described in the language manual.

The legacy root `string.h` has a three-argument `strcpy`; that is **not**
the new two-argument API. `strlen`, comparisons, bounded-copy variants, and
the rest of the standard string library are outside this migration.

### `stdio.h`: minimal character I/O interface

```c
int getchar(void);
int putchar(int character);
```

The declarations are target-independent; the supplied implementations require
`--target PC1403`. Including the header does not include both routines:
input-only programs add no output routine or LCD buffer.

The current PC-1403 adapter is not a conforming standard-C stream:
`getchar` returns raw hardware key codes, and `putchar` supports only
append-only printable output. Their exact contracts and accepted deviations
are in the target-specific section below. `puts` and full hosted-C stdio
support are not supplied.

## Target-specific library functions and adapters

### PC-1403 `getchar`: blocking raw-key input

The external module `targets\PC1403\getchar.asm` blocks until a key is pressed.
It returns the raw hardware key code, zero-extended to a word in B:A, **not
ASCII**. For example, key `1` returns 51 and key `2` returns 50. There is no
key-to-ASCII translation table. These values must not be interpreted as
standard-C character input.

### PC-1403 `putchar`: append-only LCD output

The external module `targets\PC1403\putchar.asm` uses the low byte of its
argument and appends printable ASCII bytes (32-126) to one 24-character LCD
line. It returns the byte written.

A full line or an unsupported byte returns -1 (word representation `0xFFFF`)
without changing the line. There is no scrolling, wrapping, newline/control
processing, or public line-reset API. In particular, `putchar('\n')` returns
-1; it does not start a new line. These are accepted milestone limitations,
not full stream semantics.

The runtime owns a 25-byte buffer and a length byte, initialized by loading
the binary. The line persists across program calls, including repeated BASIC
calls to the same loaded program. Reload the binary to start with an empty
output buffer.

### LCD and keyboard endpoint migration: pending

The nonstandard `lcd.h` and `key.h` migration is separate from stdio.
`lcd_on`, `lcd_off`, `lcd_cls`, `lcd_pset`, `lcd_hline`, `key_wait`,
`key_scan`, and `keybrk` are planned endpoints, **not supplied `lcc2` APIs**.
The current stdio adapters do not establish support for this full device API.
See roadmap Task 5.3.8.2 for their pending contracts and acceptance criteria.

## Nonstandard compatibility and conversion helpers

These helpers are neither standard-C interfaces nor hardware endpoints.

| Header | Current support |
|--------|-----------------|
| `compat.h` | `byte` is `unsigned char`; `word` is `unsigned int`. Typedefs only, no runtime module. |
| `int2str.h` | Migration pending. The legacy unsigned-integer conversion helper is not part of `string.h` or stdio. |

Legacy `puts.h` is also not a standard `puts` implementation; migrating its
uses to the supported stdio API remains pending under Task 5.3.8.1.

## Runtime verification

The [project README's I/O checks](../Source/C/lcc2/README.md#standard-io-runtime-test-in-mame)
cover raw input, append-only output, return values, selective module inclusion,
stack/register preservation, and BASIC responsiveness.

The [string checks](../Source/C/lcc2/README.md#string-runtime-test-in-mame)
cover actual-header preprocessing, signature errors, selective inclusion and
deduplication, user-provided implementations, and assembly without a target.
The MAME fixture covers 17 vectors: empty/260-byte strings, high bytes, page
crossings, all internal/XRAM combinations, unchanged source and destination
guards, pointer returns and dereferencing, and caller-state preservation.
The I/O and string harnesses leave MAME unpaused.
