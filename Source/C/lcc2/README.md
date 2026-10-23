# lcc2

`lcc2` is the new C implementation of the SC61860 compiler. It is being built
incrementally alongside the existing Pascal compiler, which remains the
behavioral reference.

## Build

From this directory, run:

```sh
make
```

The build requires GCC, Flex, Bison, and GNU Make, and produces `lcc2.exe`.
The project target builds the command-line compiler, AST arena, Flex scanner,
Bison parser, and the initial code-generation module. The parser can be
exercised through `lcc2_parse_string`; the backend API currently emits byte/word
constants, stack operations, add/sub operations, labels, and parameterless
functions whose bodies contain a constant-return expression. Other constructs
are rejected before any assembly is emitted.

## Compile and run the PC-1403 smoke program

From the repository root in PowerShell:

```powershell
.\Source\C\lcc2\lcc2.exe Source\C\lcc2\tests\pc1403_smoke.c -o Source\C\lcc2\build\pc1403_smoke.asm
.\pasm.exe Source\C\lcc2\build\pc1403_smoke.asm Source\C\lcc2\build\pc1403_smoke.bin
```

The CLI requires one source path and `-o` followed by an ASM path. It parses
the source directly; use `lcpp` separately if preprocessing is needed.
Compilation errors leave an existing output file untouched. Do not use the
same file for input and output.

Complete-program output requires a parameterless `main` definition. The
default origin is `0xE030`; `#pragma org(...)` accepts a nonnegative 16-bit
integer constant or `+`/`-` expression. The default startup saves/restores
registers using the Pascal wrapper and explicitly returns to the caller.
`#pragma nosave` disables preservation, but still emits the final `RTN`.
`#pragma pc` is explicitly rejected until its output semantics are implemented.
Globals, locals, general calls, and other unsupported backend constructs are
still rejected. Function labels must not collide in PASM's case-insensitive
namespace; `SREG` is reserved when preservation is enabled.

For a physical PC-1403 or BASIC-level MAME test, reserve the code area first
(see the repository README). In BASIC PRO mode use
`POKE &FF01,&30,&E8`, then `NEW`, to reserve 2 KiB; this erases the current BASIC
program. Transfer the binary to `&E030` using the existing transfer tool or
MAME debugger `load` command. Execute `CALL &E030` in BASIC RUN mode repeatedly,
then a simple BASIC calculation to confirm the interpreter is still responsive.
The program computes `42` but does not display it; the wrapper restores BA.

For a memory/register-only automated check, start a **disposable** PC-1403
MAME session with the Lua console/bridge and execute:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_smoke.lua]])
lcc2_smoke([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_smoke.bin]])
```

Inspect `lcc2_smoke_result` after execution. The harness checks BA=`42` before
restoration, stack balance, the restored register-memory dump, and ten
consecutive returns to a machine-code caller. It pauses on success or failure.
It deliberately overwrites internal RAM, CPU state, and external RAM at
`&E000`; do not run it in a session containing work you need to preserve.
This harness checks the call/return ABI, not BASIC prompt responsiveness;
the BASIC-level check above remains a separate target acceptance step.

Run the foundational checks with `make test`. Remove generated build files with
`make clean`.

## Layout

- `ast.h` defines the typed AST contract and arena allocation API.
- `ast.c` implements zero-initialized arena allocations and string copies.
- `lexer.l` and `lexer_tokens.h` define the scanner, token codes, and source
  locations. The lexer consults the parser's typedef scopes when classifying
  identifiers.
- `parser.y` builds the typed AST for declarations, function bodies, statements,
  initializers, and expressions. The parser currently accepts the supported
  subset; semantic validation and full identifier shadowing are later work.
- `codegen.h` / `codegen.c` define an explicit codegen context with assembly
  output, deterministic labels, stack-depth tracking, and a limited AST walker.
- `parser.h` exposes `lcc2_parse_string` for parser clients and tests.
- `main.c` provides source-to-ASM compilation and CLI diagnostics.
