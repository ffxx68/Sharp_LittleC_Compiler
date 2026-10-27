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

The generated image is 3,347 bytes. The Pascal demo assembles to 1,509 bytes,
so this port is 2.22 times as large; moving state to globals reduced the
previous lcc2 image by 467 bytes (about 12%). The repository's 2 KiB PC-1403
reservation example is not large enough; reserve sufficient memory before
transferring the binary.
