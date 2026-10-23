# lcc2 User Manual

## Status and scope

`lcc2` is an experimental C compiler for the Sharp SC61860, under development
alongside the original Pascal `lcc`. It is not yet a replacement for `lcc`,
and does not compile the full language described in
[the target C subset](sc61860_c_subset.md). Parser acceptance alone does not
mean a construct can be compiled.

The current implementation supports byte/word scalar variables, parameterized
functions, direct calls, assignments, and addition/subtraction. Earlier
execution checks were made in PC-1403 MAME sessions; physical-device execution
and compatibility with other Sharp models have not been verified.

See the [project README](../Source/C/lcc2/README.md) for implementation details
and test harnesses, and the [roadmap](../Source/lcc/refactor_roadmap.md) for
planned features.

## Build and command line

The build requires GCC, GNU Make, Flex, and Bison. From the repository root
in PowerShell:

```powershell
make -C Source\C\lcc2
```

Compile a source file into PASM assembly:

```powershell
.\Source\C\lcc2\lcc2.exe .\program.c -o .\program.asm
```

Assemble it separately using the existing Pascal PASM:

```powershell
.\pasm.exe .\program.asm .\program.bin
```

`lcc2` does not produce a binary directly or deploy it to the target.
`--help` and `--version` are also available. Success returns exit code 0;
compilation/I/O failures return 1, and invalid command-line usage returns 2.
Check each command's result before using its output: compilation errors leave
an existing ASM file unchanged, so that file may be stale.

Use distinct input and output files. The compiler reads source directly;
run `lcpp` separately if preprocessing is required. Automatic preprocessing,
assembler invocation, and integration with the legacy `make.bat` are not
provided.

## First program

Save this as `program.c`:

```c
#pragma org(0xE030)

unsigned int main(void)
{
    return 40 + 3 - 1;
}
```

This generates a complete program that computes 42 and returns to its caller.
It does not display the result: the startup wrapper restores the accumulator
before returning to BASIC.

Complete ASM is ordered as startup, function definitions, data, then runtime
helpers. Startup saves registers (unless `nosave`), initializes globals before
`CALL main`, restores registers, and returns without falling through into the
following sections. The `SREG` save area is placed after functions; PASM resolves
its forward references. Comparison helpers follow data and are emitted only
when needed. Function and helper labels are checked for case-insensitive
collisions. See the project README for the integrated BASIC-caller test.

## Currently supported language

| Feature | Current support |
|---------|-----------------|
| Types | `char`, `unsigned char`, `signed char`, `int`, `unsigned int`; `void` function returns |
| Functions | Byte/word parameters by value, compatible prototypes, direct calls to definitions in the same translation unit; CLI `main` must be parameterless |
| Variables | Fixed-address globals with scalar constant initialization; automatic block-scope locals with expression initializers |
| Statements | Compound blocks, expression statements, empty statements, `if`/`else`, `while`, `do`/`while`, expression-initialized `for`, `break`, `continue`, `return` |
| Expressions | Nonnegative 16-bit integer constants, character constants, scalar identifiers, direct function calls, `=`, `+`, `-`, `==`, `!=`, unsigned `<`, `<=`, `>`, `>=`, parentheses |
| Names | Case-sensitive lookup, nested block scopes, and local-over-global shadowing |
| `const` | Const-qualified scalar objects cannot be assigned after initialization |

Declarations precede statements within a block. Non-void functions require a
return expression; void functions may use `return;` or an empty body.
Statements after an unconditional return are currently rejected. A `for` step
executes after the body, and `continue` in a `for` loop targets that step.
Signed relational comparisons are rejected. Function definitions require
named scalar parameters; prototypes may omit parameter names. Calls are
left-to-right and may be nested or recursive. Argument and return values use
the byte/word stack convention described below.

Arrays, pointers, aggregates, `switch`, `goto`, static/extern objects,
external function calls, and operators other than those listed above are not
implemented in the scalar backend. Global initializers are limited to integer
and character constants combined with `+` and `-`; list and floating-point
initializers are rejected. Comparison expressions call legacy `LIB_CP*16`
routines, embedded once after generated functions when needed. Their source
is built into the compiler, so no external `.lib` files are required.
Unknown runtime requests and function names colliding with included runtime
labels are rejected before CLI output is written. In particular, use `0 - 1`
rather than unary `-1` in this initial slice.

### Types and conversions

Bytes are 8 bits; words are 16 bits. Plain `char` is unsigned. Expressions
use the word accumulator B:A; byte loads zero-extend for unsigned types and
sign-extend for `signed char`. Byte assignments retain the low byte, and
assignment expressions yield the converted stored value. Unsigned word
addition/subtraction wraps modulo 65536.

Signed overflow and reads of uninitialized locals are not diagnosed.
Negative constants via unary minus, casts, and general signed operations
remain outside the implemented subset.

### Variable placement

Globals require an explicit address:

```c
unsigned char __at(8) scratch;
__xram __at(0xE600) unsigned int result;
```

Register-space objects must fit entirely within internal RAM addresses
8 through 95. XRAM objects must fit within the 16-bit external address space.
Overlapping declarations in the same address space are rejected. Fixed-address
globals with no initializer are zeroed before `main`. Explicit scalar
initializers are constant expressions using integer/character constants and
`+`/`-`; results are truncated to the declared width (8 or 16 bits) and stored
before `main` as part of startup.

**Placement is the programmer's responsibility:** the compiler does not prove
that objects avoid generated code, the runtime stack, or BASIC/firmware state.
The startup wrapper restores registers 8-11, so stores there do not persist
after the program returns to BASIC. Use reserved external RAM for observable
results.

Local variables occupy a function-wide stack frame, including declarations
in nested blocks; slots are not reused. The current frame limit is 32 bytes.
This is an implementation limit, not a guarantee that every caller has enough
stack space for the frame, parameters, nested calls, and expression
temporaries. Parameter offsets account for both the local frame and the total
parameter size; for N local bytes and P parameter bytes, parameter i begins at
legacy offset `N + P - A`, where A is one plus the total width of preceding
parameters. The existing byte/word local access rules apply, with live
expression temporaries added to the offset. Register/XRAM words are
little-endian; stack words use the reversed-word convention. Callers remove
argument bytes after return while preserving byte/word results. Void calls
use individual `POP`s for fewer than four argument bytes and stack-pointer
adjustment for four or more.

### Target pragmas and labels

| Pragma | Current behavior |
|--------|------------------|
| `#pragma org(address)` | Sets the assembly origin; default is `0xE030`. Accepts a nonnegative 16-bit integer constant or supported `+`/`-` constant expression. |
| `#pragma nosave` | Disables the register save/restore wrapper; still returns to the caller. Use only when the calling environment permits the clobbers. |
| `#pragma pc(address)` | Recognized by the parser but rejected by program generation. |

The default wrapper follows the Pascal compiler's register preservation
sequence, not a complete CPU-state save: it sets I to 11 before saving and
also changes P and DP.

PASM labels are case-insensitive even though C identifiers are case-sensitive.
Function definitions must not collide under that rule. `SREG` is reserved
when the wrapper is enabled; names starting with `LB` followed by a digit are
reserved for generated labels.

## Run on a PC-1403 in MAME

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

For an observable result, compile this instead:

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
See [the bridge documentation](../mame_mcp_readme.md) for setup.

## Checks and examples

```powershell
make -C Source\C\lcc2 test
```

The suite covers the lexer/parser, AST, scalar resolver, emitted instruction
sequences, and CLI diagnostics. Existing target fixtures are
`Source\C\lcc2\tests\pc1403_smoke.c` and
`Source\C\lcc2\tests\pc1403_scalars.c`, with Lua harnesses documented in the
project README. Target checks use memory/register dumps rather than
screenshots; BASIC-level execution is separate from synthetic ASM call tests.
