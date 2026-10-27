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
appended; device-independent string and conversion routines remain available.

The command-line driver appends each external library module only when its
function is called in generated code, once per module. Header declarations
alone add nothing. A user-defined implementation does not select the supplied
module. Selection is based on emitted calls, not whole-program reachability.
The append-only `puts` adapter also needs `putchar`; only this required
dependency is added, unless the program defines its own `putchar`. Calling
`puts` does not add input, string-copy, or conversion routines.
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
int puts(const char *text);
```

The declarations are target-independent. The supplied `getchar` and `putchar`
implementations require `--target PC1403`. The separate `runtime\puts.asm`
adapter uses `putchar` through the same ABI and can also use a program-defined
`putchar`, including without `--target`. Without a target or such an
implementation, its output dependency is unresolved at assembly time.
Including the header alone adds nothing; input-only programs add no output
routine or LCD buffer.

The current PC-1403 adapter is not a conforming standard-C stream:
`getchar` returns raw hardware key codes, and `putchar` supports only
append-only printable output. Their exact contracts and accepted deviations
are in the target-specific section below. Full hosted-C stdio is not supplied.

**Accepted nonstandard `puts`: no trailing newline.** The current milestone
explicitly uses append-only output, not standard C's string-plus-newline
behavior. `puts` writes bytes through the string's NUL, excluding NUL, via
`putchar`, then returns 0. An empty string succeeds without writing anything,
including on a full line. The first `putchar` error stops the operation and
returns -1 (`0xFFFF`); any successfully written prefix remains on the line.
No remaining bytes are processed, and no rollback occurs.

On PC-1403, strings consisting of supported printable bytes fit only within
the remaining 24 columns. Full-line, newline/control, and non-ASCII bytes
produce the same errors as `putchar`. For example, `puts` on the brace-list
string `{65, 66, 10, 67, 0}` appends `AB`, returns -1 at the newline, and
never writes `C`. Do not assume standard `puts` newline behavior when
porting code.

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
output buffer. A separate 88-byte workspace preserves caller internal RAM
8..95 across display-buffer preparation and the ROM call, including source
strings and live stack slots. Source storage must still not overlap the
live stack itself. The output workspace and display state are shared, so the
adapter is not reentrant.

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
| `int2str.h` | Standalone unsigned-word to decimal conversion; not part of `string.h` or stdio. |

### `int2str.h`: unsigned decimal conversion

```c
void int2str(unsigned int number, char *destination);
```

Convert 0..65535 to decimal ASCII with no leading zeros or sign, followed by
NUL. Zero becomes `{48, 0}` (`0`); 65535 needs five digits plus NUL.
The caller supplies at least **six writable bytes** in internal RAM or XRAM,
avoiding live stack and code storage. There is no capacity check or
allocation, and bytes after the NUL are unchanged. The function has no return
value; negative/signed conversion is not provided.

Its separate `runtime\int2str.asm` module is selected only when externally
called, with or without `--target`. It uses no device ROM calls, static
scratch buffer, or other library module.

### Migrating legacy conversion/display calls

Keep the legacy root headers for the Pascal compiler. For `lcc2`, include
`int2str.h` for conversion and replace `puts.h` with `stdio.h`. Pass a
character pointer instead of an integer holding an address:

```c
#include "stdint.h"
#include "int2str.h"
#include "stdio.h"

__xram __at(0xE700) char buffer[6];
__xram __at(0xE708) int display_result;

void main(void)
{
    uint16_t number = 65535;
    int2str(number, &buffer[0]);
    display_result = puts(&buffer[0]);
}
```

Preprocess with the supplied include directory and compile with
`--target PC1403` for the supplied output backend. Unlike the legacy ROM
display operation, this `puts` appends to the existing line, returns an
explicit success/error result, and never adds newline. The runtime fixture
covers this conversion-to-display migration. Legacy examples outside the
`lcc2` tree continue to use their original headers and compiler.

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

The [library-completion checks](../Source/C/lcc2/README.md#library-completion-test-in-mame)
preprocess all supplied headers twice to check guards, compile typedef uses
and signature errors, and verify independent modules plus the `puts` output
dependency. Runtime vectors cover unsigned decimal boundaries, internal/XRAM
destinations, page crossings, output success/error prefixes, shared
`puts`/`putchar` state, and the conversion/display example.
