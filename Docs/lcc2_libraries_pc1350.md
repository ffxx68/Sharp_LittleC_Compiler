# lcc2 PC-1350 Library Reference

Compile with `--target PC1350` to select these implementations. Include
`key.h`, `lcd.h`, `stdio.h`, or `pc1350.h` for their declarations. Only
called functions are included; the target option does not reserve memory.

## Keyboard and display

| Function | Behavior |
|---|---|
| `unsigned char key_scan(void)` | Calls the keyboard ROM once. Returns `0xFF` when no key is pressed; otherwise returns raw code `0x00..0x3F`, or `0x40` for multiple keys. |
| `unsigned char key_wait(void)` | Polls until a key is pressed. Key code zero and the multiple-key code are valid results. Does not wait for release. |
| `int getchar(void)` | Uses `key_wait`; returns the raw key code zero-extended to `int`, not ASCII. No echo or EOF. |
| `void lcd_on(void)` | Enables the display without changing display memory. |
| `void lcd_off(void)` | Disables the display without changing display memory. |

The keyboard functions use private save state; the LCD functions share save
state. These functions are non-reentrant and preserve caller internal RAM
8..95. `keybrk`, `putchar`, and public pixel functions are not supplied.

## `pc1350.h`: ROM-backed string and BCD operations

The APIs use the firmware string buffer at `PC1350_STRING_BUFFER` (`0x6E60`),
with capacity `PC1350_STRING_CAPACITY` (80 bytes). Declare it as an XRAM
array if direct access is needed. Payload lengths are explicit; strings in
this buffer are not NUL-terminated.

| Function | Input and result |
|---|---|
| `int pc1350_str(const unsigned char *number)` | Formats a valid 8-byte ROM-format BCD number as BASIC `STR$` text in the firmware buffer; returns the length without a C NUL terminator. |
| `int pc1350_val(unsigned char length, unsigned char *number)` | Parses 1..79 printable ASCII bytes at the buffer start into an 8-byte BCD number; returns 0 on success. |
| `int pc1350_compare(unsigned char operation, unsigned char left_length, unsigned char right_length)` | Compares consecutive printable-ASCII payloads in the buffer (combined length at most 80; empty strings allowed); returns Boolean 0 or 1. |

For comparison, use `PC1350_COMPARE_EQ`, `_NE`, `_LT`, `_GT`, `_LE`, or
`_GE`. This is a predicate result, not `strcmp` ordering. `VAL` follows
BASIC parsing rules rather than strict C numeric parsing. Failed conversions
leave the destination number unchanged.

All three calls return a negative error code on failure:

| Code | Meaning |
|---|---|
| `PC1350_ROM_UNSUPPORTED` (-1) | ROM version is not recognized; versions 0 and 1 are supported. |
| `PC1350_ROM_INVALID_ARGUMENT` (-2) | Length, operation, pointer range, or input bytes are invalid. |
| `PC1350_ROM_CONVERSION_ERROR` (-3) | ROM `VAL` reports a conversion error. |

ROM-version errors take precedence over argument errors. Number pointers
must address 8 bytes in internal RAM 8..95 or external RAM, without
overlapping live stack or code. The routines preserve caller internal RAM
and restore the firmware string-buffer pointer, but overwrite the string
buffer and may alter other firmware scratch and display RAM. They are
non-reentrant; do not call while BASIC or another firmware operation is using
that workspace. Keep program code, save state, and number storage below
`0x6C30`, outside the system area. The default origin `0x6900` is an example,
not a reservation.

Runtime checks cover the supplied ROM version 1 in MAME. ROM version 0,
physical devices, and keyboard endpoints have not been runtime-verified.
See the [PC-1350 machine-language manual](../Source/C/lcc2/targets/PC1350_MachineLanguage_EN.docx)
and the [project tests](../Source/C/lcc2/README.md) for implementation and
validation details.
