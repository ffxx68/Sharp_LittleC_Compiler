# lcc2

`lcc2` is an experimental C compiler for Sharp SC61860 computers, developed
alongside the original Pascal compiler.

See the [language manual](../../../Docs/lcc2_manual.md) for supported C
features and compiler usage, the [library reference](../../../Docs/lcc2_libraries.md)
for shared library APIs, and the target references for [PC-1350](../../../Docs/lcc2_libraries_pc1350.md),
[PC-1401](../../../Docs/lcc2_libraries_pc1401.md), and
[PC-1403](../../../Docs/lcc2_libraries_pc1403.md).

## Build and test

The build requires GCC, Flex, Bison, GNU Make, and a POSIX-compatible shell.
On Windows, use an MSYS2/MinGW shell. From this directory:

```sh
make
make test
```

`make test` builds the compiler and local assembler, then runs the automated
compiler and library tests. From the repository root, the same suite can be
run with:

```powershell
make -C Source\C\lcc2 test
```

The target fixtures include `tests\smoke.c` and `tests\scalars.c`, with Lua
harnesses for MAME checks. Automated target checks inspect memory and
registers; BASIC-level execution is a separate check.

## Compile a program

Preprocess C source separately when using headers, then compile it to PASM
assembly and assemble the result. For example, from the repository root in
PowerShell:

```powershell
gcc -E -P -nostdinc -I Source\C\lcc2\include Source\C\lcc2\tests\smoke.c -o Source\C\lcc2\build\smoke.i
.\Source\C\lcc2\lcc2.exe --target PC1403 Source\C\lcc2\build\smoke.i -o Source\C\lcc2\build\smoke.asm
.\pasm.exe Source\C\lcc2\build\smoke.asm Source\C\lcc2\build\smoke.bin
```

The compiler accepts one input file and `-o` with an assembly output path.
Complete programs require a parameterless `main`. The default origin is
`0xE030`; use `--target PC1350`, `PC1401`, or `PC1403` to select device
libraries and that target's default origin. `#pragma org(...)` overrides the
origin. Selecting a target does not reserve memory, so ensure the program and
its data fit in an area reserved for the binary. See the [language manual](../../../Docs/lcc2_manual.md)
for deployment instructions.

## Runtime checks

Target-specific test fixtures and MAME instructions are maintained with the
source under `tests`. Run them only in a disposable emulator session or after
saving any work that may be overwritten. The PC-1403 smoke fixture can be
assembled with the commands above; in the MAME Lua console, load
`tests\smoke.lua` and call `lcc2_smoke` with the binary's absolute path.

Library behavior and its known target limitations are documented in the
[library references](../../../Docs/lcc2_libraries.md).

### Run a program on PC-1403 in MAME

Use a disposable session or save any BASIC program first. In BASIC **PRO**
mode enter, confirming each line:

```text
NEW
POKE &FF01,&30,&E8
NEW
MEM
```

This erases BASIC and moves its start to `&E830`, reserving 2 KiB beginning
at `&E030`. On a standard PC-1403, `MEM` should report 4830. Keep the binary
and any XRAM test outputs entirely inside the reserved area.

In the **MAME debugger console**, load the assembled binary:

```text
load "C:\path\to\program.bin",e030:maincpu
```

Then enter this caller in BASIC PRO mode:

```basic
10 CALL &E030
20 END
```

Switch to BASIC **RUN** mode and execute `RUN`. Repeat it, verify the program
is retained with `LIST` in PRO mode, and check that BASIC still evaluates a
simple calculation. Do not repeat the reservation sequence afterward: `NEW`
would intentionally erase the BASIC program.

To inspect an observable result, compile a program that writes to reserved
XRAM, for example:

```c
#pragma org(0xE030)
__xram __at(0xE600) unsigned int result;

unsigned int main(void)
{
    unsigned int value = 40;
    result = value + 2;
    return result;
}
```

After execution, a memory dump at `&E600` should contain `2A 00`. Confirm
that the binary ends before `&E600` so the result does not overwrite code.

### Keyboard automation

Load `pc1403_key.lua` in MAME's Lua console to use `keyline()` (a command plus
ENTER) or `keyfile()` (commands from a file). The shortcuts `_` and `|` select
CALC and cycle BASIC modes respectively; establish the starting mode before
replaying commands. Run functions that use `emu.wait` in a coroutine, as in
the project's Lua test harnesses.

Remote tool control additionally requires `mame_mcp_bridge.lua` in the
intended session. Use only one bridge-enabled MAME instance for a shared
command directory; otherwise commands can reach the wrong emulator.
See the [bridge documentation](../../../mame_mcp_readme.md) for setup.
