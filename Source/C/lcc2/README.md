# lcc2

`lcc2` is the new C implementation of the SC61860 compiler. It is being built
incrementally alongside the existing Pascal compiler, which remains the
behavioral reference.

See the [user manual](../../../Docs/lcc2_manual.md) for language support,
command-line usage, and deployment instructions.

## Build

From this directory, run:

```sh
make
```

The build requires GCC, Flex, Bison, and GNU Make, and produces `lcc2.exe`.
The project target builds the command-line compiler, AST arena, Flex scanner,
Bison parser, scalar resolver, and code-generation module. The parser can be
exercised through `lcc2_parse_string`; the backend emits byte/word constants,
scalar access, stack operations, add/sub/comparison operations, labels,
structured control flow, and parameterized functions with direct calls. The
resolver validates the supported subset before emission.

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
Comparison expressions call the legacy `LIB_CP*16` helpers. Required helpers
are deduplicated in the context and embedded after generated functions from
a built-in catalog; no external `.lib` files are needed. Unknown helper
requests and function names colliding with included runtime labels fail.
Direct calls to
defined functions use byte/word parameters by value; unresolved external calls
and other unsupported backend constructs are rejected.
Function labels must not collide in PASM's case-insensitive namespace;
`SREG` is reserved when preservation is enabled, and `LB` followed by digits
is reserved for compiler labels.

## Scalar subset and layout

`symtab.c` resolves declarations and identifier bindings independently of
instruction emission. Names are case-sensitive, with block scopes and local
shadowing. Byte/word scalar declarations, local expression initializers,
plain assignments, `+`/`-`, equality, unsigned relational comparisons,
conditionals, `while`, `do`/`while`, and expression-initialized `for` loops are
supported, including `break` and `continue`. Functions accept named byte/word
parameters by value and support direct calls, including recursion and nested
calls. Prototypes must match definitions; calls require a definition in the
translation unit. The entry-point `main` remains parameterless. A `for` step
executes after its body, and `continue` targets the step. Signed relational
comparisons are rejected. Non-void functions require a return. Static/extern
storage, arrays, pointers, aggregates, and statements after a return are not
yet supported by the resolver.

Globals require an explicit constant `__at` placement. Internal-RAM globals
must fit in `[8,96)`; cells 0-7 are reserved for arithmetic/addressing registers.
External-RAM globals use `__xram __at(address) type name`. Register and XRAM
address spaces are checked separately for overlapping objects. Global
initializers are emitted before `main`: integer/character constant expressions
are converted to the declared byte/word width, and objects without explicit
initializers are zeroed. Automatic global allocation remains deferred. The
caller must avoid placing objects
over live stack slots, generated code, or BASIC/firmware memory. Registers
8-11 are included in the existing startup save/restore wrapper, so changes
there do not persist after returning to BASIC.

Local bytes and words use one function-wide frame, including declarations in
nested blocks, without reusing slots. The local frame is limited to 32 bytes;
actual safe stack capacity still depends on the caller, parameters, nested
calls, and live expression temporaries.
For a frame of N bytes, an object whose cumulative allocation ends at C has
legacy local offset `N-C-2` for a byte or `N-C-1` for a word. Its byte (or word
high byte) is at `R + N-C + temporary_depth`. Words are reversed on the stack:
the low byte follows the high byte. Register and XRAM words are little-endian.
Frame allocation/cleanup is owned by codegen, not the resolver. Codegen tracks
frame bytes separately from expression temporaries and reports their total
through `codegen_stack_depth`.

Arguments are evaluated left to right and pushed in parameter order. Callees
access parameters at their computed stack offsets; callers remove argument
bytes after return while preserving byte/word results. Void-call cleanup uses
individual pops below four bytes and adjusts the stack pointer at four bytes
or more. General external-function linkage remains deferred.

Expressions are evaluated in the word accumulator B:A. Byte loads are promoted
(plain `char` and `unsigned char` zero-extend, `signed char` sign-extends).
Byte assignment keeps the low byte, and an assignment expression yields the
converted stored value rather than the clobbered registers of a store helper.
Signed overflow and uninitialized local reads are not diagnosed.

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

## Scalar functional test in MAME

Build `tests\pc1403_scalars.c` using the same CLI/PASM commands as the smoke
fixture, substituting `pc1403_scalars` for `pc1403_smoke`. In a **disposable**
PC-1403 session with the bridge and only one bridge-enabled MAME instance,
load `pc1403_key.lua`, then:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_scalars.lua]])
lcc2_scalar_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_scalars.bin]])
```

The script clears BASIC, reserves the ASM area, enters a two-line BASIC
caller through the emulated keyboard, deploys the binary, and executes `RUN`
ten times. Inspect `lcc2_scalar_result`: success requires `status="PASS"`,
`calls=10`, bytes `2A 00 3B FF` at `&E600`, and `5E 12` at `&E7FF`, with
the BASIC program unchanged. No screenshots or machine snapshots are used.

## Comparison runtime test in MAME

Compile and assemble `tests\pc1403_comparisons.c` as above. In a **disposable**
PC-1403 session, load `tests\pc1403_comparisons.lua` and call
`lcc2_comparison_test` with the binary's absolute path. Inspect
`lcc2_comparison_result`: success is `status="PASS"`, `calls=130`, and
`vectors=13`. The harness injects operands immediately before startup calls
`main`, checks all six unsigned comparisons across equality and byte/word
boundaries, and verifies stack balance and register restoration on every
return. It repeats each vector ten times and pauses on success or failure.
It overwrites CPU state, RAM at `&E000`, generated code, and test globals at
`&F000`/`&F010`; it does not test BASIC prompt responsiveness.

`codegen_runtime_output` exposes resolved runtime source independently of
function output. `codegen_translation_unit` leaves it separate; complete
`codegen_program` appends it once in catalog order after generated functions.
`codegen_require_runtime` reports an error for unknown routine names.

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
  subset; the scalar resolver supplies the currently implemented semantic checks.
- `symtab.h` / `symtab.c` define scalar bindings, scope resolution, and storage
  layout. Full symbol-table support remains incremental.
- `codegen.h` / `codegen.c` define an explicit codegen context with assembly
  output, deterministic labels, stack-depth tracking, and a limited AST walker.
- `parser.h` exposes `lcc2_parse_string` for parser clients and tests.
- `main.c` provides source-to-ASM compilation and CLI diagnostics.
