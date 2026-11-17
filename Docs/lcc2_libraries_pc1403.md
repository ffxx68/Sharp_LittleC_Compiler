# lcc2 PC-1403 Library Reference

Compile with `--target PC1403` to select these implementations. Include
`key.h`, `lcd.h`, or `stdio.h` for their declarations. Only called functions
are included; the target option does not reserve memory.

## Keyboard

| Function | Behavior |
|---|---|
| `unsigned char key_scan(void)` | Nonblocking; returns `0xFF` when no key is pressed, otherwise a raw code from 0 to 83. |
| `unsigned char key_wait(void)` | Blocks until a key is pressed and returns its raw code. |
| `unsigned char keybrk(void)` | Nonblocking; returns 1 while the break key is pressed, otherwise 0. |
| `int getchar(void)` | Blocks for a key and returns its raw code zero-extended to `int`, not ASCII. |

There is no key-to-ASCII translation. `getchar` uses `key_wait`; code zero is
a valid key result.

## Character output

| Function | Behavior |
|---|---|
| `int putchar(int character)` | Appends the low byte if it is printable ASCII (32..126) and the 24-character line is not full; returns the byte written. |
| `int puts(const char *text)` | See the [common `puts` contract](lcc2_libraries.md#stdioh-minimal-character-io). |

`putchar` returns -1 (`0xFFFF`) for a full line or unsupported byte, without
changing the line. It does not scroll, wrap, process newlines or controls, or
expose a line-reset API.
The 25-byte output buffer is initialized when the binary is loaded; its
contents persist across calls to the loaded program. Reload the binary to
start a new line. The output workspace is shared, so the routine is
non-reentrant.

## LCD controls and pixels

The LCD is 120 pixels wide by 7 high, with `(0, 0)` at the bottom-left.

| Function | Behavior |
|---|---|
| `void lcd_on(void)` / `void lcd_off(void)` | Enable or disable the display. |
| `void lcd_cls(unsigned char mode)` | Mode 0 clears the pixel area; mode 1 fills it. Other modes do nothing. |
| `void lcd_pset(unsigned char x, unsigned char y, unsigned char mode)` | Sets or clears one pixel (mode 1 or 0). Invalid coordinates or modes do nothing. |
| `void lcd_hline(unsigned char x, unsigned char y, unsigned char length, unsigned char mode)` | Sets or clears pixels from `x` through `x + length - 1`, clipped at the right edge. Zero length, invalid `y`, or invalid modes do nothing. |

LCD endpoints share save state and are non-reentrant. They preserve caller
internal RAM and stack state. The I/O fixtures in the
[project README](../Source/C/lcc2/README.md) cover MAME behavior; physical
device behavior is not verified.
