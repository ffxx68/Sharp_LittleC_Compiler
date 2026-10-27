# Bounce

This is the `Demos/bounce` PC-1403 demo ported to `lcc2`. The left and right
cursor keys move the paddle; press the break key to exit. The timer-based
random seed from the original assembly version is replaced with a portable
8-bit loop counter. Game state is allocated in fixed internal-RAM locations
16 through 40 rather than on the stack.

From the repository root in PowerShell, preprocess, compile, and assemble:

```powershell
gcc -E -P -nostdinc -I Source\C\lcc2\include Source\C\lcc2\demos\bounce\main.c -o Source\C\lcc2\build\bounce.c
.\Source\C\lcc2\lcc2.exe --target PC1403 Source\C\lcc2\build\bounce.c -o Source\C\lcc2\build\bounce.asm
.\pasm.exe Source\C\lcc2\build\bounce.asm Source\C\lcc2\build\bounce.bin
```

The current generated image is 2,927 bytes. The Pascal demo assembles to 1,509
bytes, so this port is 1.94 times as large. Moving state to globals reduced the
earlier lcc2 image by 467 bytes (about 12%); Task 6.2 backend optimizations
removed another 156 bytes, and sharing the LCD endpoints' 88-byte save area
removed 264 bytes from the four LCD routines used here. The repository's 2 KiB
PC-1403 reservation example is not large enough; reserve sufficient memory
before transferring the binary.
