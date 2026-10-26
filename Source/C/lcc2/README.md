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
shadowing. Byte/word scalar declarations, single-dimensional byte/word arrays
with declared or inferred extents, brace-list initialization, and
constant and runtime-index reads/writes are supported, alongside local expression
initializers, assignments (plain and compound), `++`/`--`, `+ - * / %`,
`& | ^ ~ << >>`, `!`, `&&`, `||`, the comma operator, equality, unsigned
relational comparisons, structures with `.`/`->` member access, conditionals, `while`, `do`/`while`, and expression-initialized
`for` loops, `switch`/`case`/`default`, and correctly nested `break` and
`continue`. Functions accept named byte/word
parameters by value and support direct calls, including recursion and nested
calls. Prototypes must match definitions; calls require a definition in the
translation unit. The entry-point `main` remains parameterless. A `for` step
executes after its body, and `continue` targets the step. Signed relational
comparisons, `/`, `%` and `>>` are rejected unless an operand is provably
unsigned or nonnegative (signed semantics are deferred to roadmap 5.3.9).
Compound/increment operands must be side-effect-free lvalues. Non-void
Functions require a return. Static/extern storage, unions, and
statements after a return are not yet supported by the resolver. Byte/word pointers to globals or constant-
index elements of global arrays in register or XRAM storage are 16-bit values;
pointer locals and fixed-address global pointer objects support initialization,
assignment, copying, null values, and by-value function parameters. Indirect
byte/word loads and stores are supported for pointers to register and XRAM
objects, including pointers received as parameters. The high byte distinguishes
register addresses from XRAM addresses, so address-of on XRAM objects below 256
is rejected. Global pointer initialization is limited to null constants.
Addresses of automatic locals, whole arrays, dynamic array elements, and
functions are unsupported, as are pointer-to-pointer types and pointer returns.
Pointer arithmetic is limited to `++ -- += -=` on plain pointer variables,
scaled by the pointee size.
Array extents must be positive
constants (or inferred from a nonempty initializer); multidimensional arrays
remain unsupported.

Array subscripts accept supported integer expressions, including calls and
nested subscripts. Constant out-of-range subscripts are rejected. Runtime
bounds are not checked: an out-of-range runtime subscript has undefined
behavior, not a zero result or ignored store. Runtime addressing retains the
full 16-bit index, scales word offsets by two, and evaluates each index once.
Register/XRAM elements are little-endian; local words use the reversed stack
byte order described below. Array storage consumes the full extent in overlap
and local-frame checks.

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
Contiguous zero runs of at least four bytes use shared internal-RAM or XRAM
fill routines; shorter runs stay inline. This applies to global arrays and
initialized local arrays, while uninitialized local arrays remain untouched.

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

## Pointer runtime test in MAME

Compile and assemble `tests\pc1403_pointers.c`, then in a **disposable**
bridge-enabled PC-1403 session running BASIC, load `pc1403_key.lua` and run:

If a previous test injected CPU state directly, use the emulated device Reset
key first to restore a clean BASIC environment; a machine soft reset may not
be sufficient.

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_pointers.lua]])
lcc2_pointer_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_pointers.bin]])
```

If the debugger is stopped, resume execution with F5. Inspect
`lcc2_pointer_result`: success requires `status="PASS"`, `calls=10`,
`returns=10`, and `basic_checks=10`.
The harness checks indirect byte/word register and XRAM accesses, pointer
parameters, assignment-expression conversion/results, signed-byte promotion,
and an XRAM word crossing `&E5FF`. The eight output words at `&E700` must be
`0007 0007 BEEF 00A5 1234 FF80 D156 1234`. Every call checks stack balance
and wrapper register restoration. The harness clears BASIC, reserves the ASM
area, and enters a two-line BASIC caller through the keyboard. After each
`RUN`, it executes `POKE&E710,6*7` and verifies the value 42, the unchanged
BASIC program, and the BASIC memory pointer. It pauses on success or failure.
Use only a disposable session: this clears the current BASIC program and
overwrites reserved test RAM.

## Structure pointer-member runtime test in MAME

Compile and assemble `tests\pc1403_struct_members.c`, then in a **disposable**
bridge-enabled PC-1403 session running BASIC, load the repository-root
`pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_struct_members.lua]])
lcc2_struct_member_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_struct_members.bin]])
```

The fixture checks byte and word member reads/writes through `->` for fixed
internal-RAM and XRAM structures, including stack balance, register
restoration, and BASIC responsiveness across ten calls. Resolver and codegen
tests also cover accesses through structure-pointer parameters. Success
requires `status="PASS"`, `calls=10`, `returns=10`, and `basic_checks=10`; the
six output bytes at `&E700` must be `EF BE 7C 00 34 12`. Use only a disposable
session: this clears the current BASIC program and overwrites reserved test
RAM.

## Bitwise and unary operator runtime test in MAME

Compile and assemble `tests\pc1403_bitwise_unary.c`, then in a **disposable**
bridge-enabled PC-1403 session load `pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_bitwise_unary.lua]])
lcc2_bitwise_unary_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_bitwise_unary.bin]])
```

The fixture computes 16 word results at `&E720` covering `& | ^ ~ - + !`.
Success requires `status="PASS"`; the 32 output bytes must match `EXPECTED`
in the Lua file. This clears the current BASIC program.

## Shift operator runtime test in MAME

Compile and assemble `tests\pc1403_shifts.c`, then in a **disposable**
bridge-enabled PC-1403 session load `pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_shifts.lua]])
lcc2_shifts_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_shifts.bin]])
```

The fixture checks `<<` and `>>` with constant, byte and word counts
(including 0, 8, 15, 16 and 256) at `&E720`. Success requires
`status="PASS"`. Signed `>>` is rejected until signed semantics are added.

## Logical operator runtime test in MAME

Compile and assemble `tests\pc1403_logical.c`, then in a **disposable**
bridge-enabled PC-1403 session load `pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_logical.lua]])
lcc2_logical_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_logical.bin]])
```

The fixture checks `&&`, `||` and comma values, short-circuit side effects,
precedence and use in `if`/`while` conditions (results at `&E720`). Success
requires `status="PASS"`.

## Multiply/divide runtime test in MAME

Compile and assemble `tests\pc1403_muldiv.c`, then in a **disposable**
bridge-enabled PC-1403 session load `pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_muldiv.lua]])
lcc2_muldiv_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_muldiv.bin]])
```

The fixture checks `*`, `/` and `%` (truncation, zero operands, runtime zero
divisor, mixed expressions) with results at `&E720`. Success requires
`status="PASS"`. `/` and `%` on signed operands are rejected until signed
semantics are added.

## Switch runtime test in MAME

Compile and assemble `tests\pc1403_switch.c`, then in a **disposable**
bridge-enabled PC-1403 session load `pc1403_key.lua` and run:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_switch.lua]])
lcc2_switch_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_switch.bin]])
```

The fixture checks byte and staged word dispatch, single evaluation,
fallthrough, default selection, nested switches, `break`, and `continue`
targeting the enclosing loop. Results are checked at `&E720`; `&E740` records
the controlling-expression call count. Success requires `status="PASS"` after
ten runs with BASIC register restoration and responsiveness verified.

## Compound assignment and increment/decrement runtime test in MAME

Compile and assemble `tests\pc1403_update_ops.c`, `pc1403_incdec.c`,
`pc1403_update_arrays.c` and `pc1403_update_members.c`, then in a
**disposable** bridge-enabled PC-1403 session load `pc1403_key.lua` and run
each harness, for example:

```lua
dofile([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\tests\pc1403_incdec.lua]])
lcc2_incdec_test([[C:\path\to\Sharp_LittleC_Compiler\Source\C\lcc2\build\pc1403_incdec.bin]])
```

The harnesses are `lcc2_update_ops_test`, `lcc2_incdec_test`,
`lcc2_update_arrays_test` and `lcc2_update_members_test`. They check all
compound operators, prefix/postfix values, byte wrap-around, array, pointer
(scaled) and member updates (results at `&E720`). Success requires
`status="PASS"`.

## Complete-program output and integration test

`codegen_program` retains explicit startup, function, data, and runtime
sections until final composition. `codegen_section_output` exposes each
section independently. PASM source is concatenated in this order:

1. Header/origin, optional register save, fixed-address global initialization
   in declaration order, `CALL main`, optional register restore, and `RTN`.
2. Function definitions in translation-unit order, with generated `LB` labels.
3. Data declarations: currently the twelve-byte `SREG` save area, omitted
   with `#pragma nosave`. Fixed-address globals need startup stores, not
   symbolic data declarations; arrays are fixed-address globals initialized
   through startup stores, and automatic global allocation remains unsupported.
4. Required comparison routines in catalog order, each emitted once.

The startup `RTN` prevents falling through into functions or data. SREG and
runtime references may point forward, as supported by PASM. Function names
are checked against compiler labels, SREG (when present), and all labels of
included runtime routines in PASM's case-insensitive namespace. Runtime text
remains independent of generated function text. CLI output is opened only
after successful generation/composition, so compilation failures preserve
existing output.

Compile and assemble `tests\pc1403_program.c` to exercise initialized/zeroed
register and XRAM globals, nested stack frames, calls, comparisons, and a
loop. In a fresh **disposable** PC-1403 session, load `pc1403_key.lua`, then
`tests\pc1403_program.lua`, and invoke `lcc2_program_test` with the binary's
absolute path. This erases BASIC, reserves two KiB for ASM, enters a BASIC
caller, and performs ten `RUN`s. Each run checks outputs `37 12 23 00 FF FF FF`
at `&E600`, startup/return stack equality, register restoration, and BASIC
memory preservation. A subsequent keyboard-entered `POKE&E607,42` verifies
that BASIC has resumed interpreting commands. Success is
`lcc2_program_result.status="PASS"`, `calls=10`, `returns=10`, with final
outputs `37 12 23 00 FF FF FF 2A`; the emulator pauses at completion.

## Array functional tests in MAME

Compile and assemble `tests\pc1403_arrays.c` and
`tests\pc1403_arrays_wide.c`. In a **disposable** PC-1403 session, load
`pc1403_key.lua` and `tests\pc1403_arrays.lua`, then invoke
`lcc2_array_test` with the first binary's absolute path. The test clears BASIC
and reserves four KiB; it runs ten BASIC calls covering byte/word register,
XRAM, and local arrays, page crossings, zero initialization, signed-byte
promotion, assignment values, and single evaluation of an index function.
It also checks the returned word, stack balance, register restoration, BASIC
program retention, and a keyboard-entered `POKE` after each return.

After success, resume emulation and invoke `lcc2_array_test` with the second
binary's path and `true` as its second argument. This reserves six KiB and
checks a byte index of 256 and a word index of 128 (byte offset 256), including
expression indices and assignment truncation. Both tests pause at completion;
require `lcc2_array_result.status="PASS"`, `calls=10`, and `returns=10` for
each run. The result table is replaced when starting the next test.

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
