# Bounce for PC-1350

This demo draws a short bouncing snake and an automatically sweeping 9-pixel paddle
directly into the PC-1350's LCD display memory. The snake reflects from the
screen edges; a paddle hit near the bottom also changes its horizontal
direction. Each paddle step clears only its trailing pixel and sets its new
leading pixel. Paddle movement is automatic; no keys steer it. Press any key to
stop the animation. It uses the PC-1350 `lcd_pset()` runtime. Code starts at
`&6030` and must remain below `&6C30`, before the system area and LCD memory.
Ensure this range is available before loading it.

From the repository root in PowerShell, preprocess, compile, and assemble:

```powershell
gcc -E -P -nostdinc -I Source\C\lcc2\include Source\C\lcc2\demos\bounce_pc1350\main.c -o Source\C\lcc2\build\bounce_pc1350.c
.\Source\C\lcc2\lcc2.exe --target PC1350 Source\C\lcc2\build\bounce_pc1350.c -o Source\C\lcc2\build\bounce_pc1350.asm
.\pasm.exe Source\C\lcc2\build\bounce_pc1350.asm Source\C\lcc2\build\bounce_pc1350.bin
```

Load the binary at `&6030` in MAME and call `&6030` from BASIC.
