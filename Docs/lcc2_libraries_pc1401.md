# lcc2 PC-1401 Library Reference

Compile with `--target PC1401` to select these implementations. Include
`key.h`, `lcd.h`, or `stdio.h` for their declarations. Only called functions
are included; the target option does not reserve memory.

## Keyboard

| Function | Behavior |
|---|---|
| `unsigned char key_scan(void)` | Returns a raw key-matrix code from 0 to 76, or `0xFF` when no key is pressed. |
| `unsigned char key_wait(void)` | Polls until a key is pressed; key code zero is valid. |
| `int getchar(void)` | Uses `key_wait` and returns the raw code zero-extended to `int`, not ASCII. No echo, line buffering, translation, or EOF. |

Examples of raw codes include `A` = 29, `ENTER` = 25, and `BRK` = 76.
The routines do not wait for key release. Keyboard modules have private
save state; the LCD endpoints share a save area. These functions preserve
caller internal RAM 8..95 and are non-reentrant.

## Display

| Function | Behavior |
|---|---|
| `void lcd_on(void)` | Enables the display with its current contents. |
| `void lcd_off(void)` | Disables the display without clearing display memory. |

PC-1401 pixel APIs are not supplied. The `pc1401_pixels` fixture writes
directly to LCD memory for testing; it is not a public pixel interface.

The keyboard and display endpoints have not been runtime-tested in MAME or
on physical hardware. The [project README](../Source/C/lcc2/README.md)
documents available fixtures and build checks.
