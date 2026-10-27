# lcc2 User Manual

## Status and scope

`lcc2` is an experimental C compiler for the Sharp SC61860, under development
alongside the original Pascal `lcc`. It is not yet a replacement for `lcc`,
and does not compile the full language described in
[the target C subset](sc61860_c_subset.md). Parser acceptance alone does not
mean a construct can be compiled.

The goal is a small, practical C subset suited to the SC61860's limited
resources, not full C89 conformance. Language features are added when they
provide useful value for target programs and can be supported reliably by the
compiler and target runtime. Unsupported features and implementation limits
are listed below; programs should stay within those limits rather than assume
that a construct accepted by another C compiler will work here.

The current implementation supports byte/word scalars and arrays, parameterized
functions, direct calls, assignments, arithmetic, bitwise, shift, logical and
comparison operators on unsigned values, byte/word pointers, structure-pointer
member access, and structured control flow.
Earlier execution checks were made in PC-1403 MAME sessions; physical-device
execution and compatibility with other Sharp models have not been verified.

**Unsigned only for now:** integer operators are implemented with unsigned
semantics. Signed operands are accepted only where the result is the same as
unsigned (for example `+`, `-`, `&`, `|`, `^`, `<<`, equality, and operands the
compiler can prove nonnegative). Signed relational comparisons, signed `>>`,
and signed `/` and `%` are rejected with a diagnostic. Full signed semantics
are planned for roadmap Task 5.3.9.

See the [library reference](lcc2_libraries.md) for standard-library subsets,
target-specific functions, and compatibility helpers; the
[project README](../Source/C/lcc2/README.md) for implementation details and
test harnesses; and the [roadmap](../Source/lcc/refactor_roadmap.md) for planned
features.

## Build and command line

The build requires GCC, GNU Make, Flex, and Bison. From the repository root
in PowerShell:

```powershell
make -C Source\C\lcc2
```

Compile a source file into PASM assembly:

```powershell
.\Source\C\lcc2\lcc2.exe --target PC1403 .\program.c -o .\program.asm
```

Assemble it separately using the existing Pascal PASM:

```powershell
.\pasm.exe .\program.asm .\program.bin
```

`--target` is optional. When provided, it selects external target runtime
assembly, maintained outside the code generator and appended by the
command-line driver. Without a target, no device-specific runtime is appended;
device-independent library routines remain available. Header preprocessing,
per-function module inclusion, library contracts, and target-specific
limitations are documented in the [library reference](lcc2_libraries.md).

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
| Types | 8-bit `char`, `unsigned char`, `signed char`; 16-bit `int`, `unsigned int`, `signed int`; `void` function returns; `const` objects |
| Functions | Byte/word and supported pointer parameters/returns, compatible prototypes, direct calls including external declarations, nested and recursive calls; CLI `main` must be parameterless |
| Variables and arrays | Fixed-address globals, automatic locals, and single-dimensional byte/word arrays; scalar and array brace-list initialization |
| Structures and pointers | Restricted structures and byte/word pointers, including assignment, copying, null values, parameters/returns, indirect loads/stores, and `->` member access through pointers |
| Statements | Compound blocks, expression and empty statements, `if`/`else`, `while`, `do`/`while`, expression-initialized `for`, `switch`/`case`/`default`, `break`, `continue`, `return`, and labels |
| Expressions | 16-bit integer and character constants, identifiers, array subscripts, direct calls, arithmetic, bitwise, shifts, logical and comparison operators, assignment and compound assignment, `++`/`--`, unary operators, comma, and parentheses |
| Names | Case-sensitive lookup, nested block scopes, and local-over-global shadowing |
| Placement extensions | `__at`, `__xram`, `#pragma org`, and `#pragma nosave` |

Declarations precede statements within a block. Non-void functions require a
return expression; void functions may use `return;` or an empty body.
Statements after an unconditional return are currently rejected. A `for` step
executes after the body, and `continue` in a `for` loop targets that step.
Function definitions require named scalar parameters; prototypes may omit
parameter names. Calls are left-to-right. Argument and return values use the
byte/word stack convention described below.

**Unsupported (relative to standard C89):** `short`, `long`, `double`,
floating-point operations, `volatile`, `auto`, `register`, `static` and
`extern` storage, block-scoped typedef declarations, `enum`, `union`, `goto`,
casts, `sizeof`, the conditional `?:` operator, and string literals.
Translation-unit typedefs for the supported types and declared external
function calls are supported; only known library implementations are appended
automatically, not arbitrary external objects. `lcc2` is not a full C89
implementation; preprocessing must be run separately.

Structures support byte/word members and member access, but nested structures,
array or pointer members, bit-fields, aggregate initialization, structure
assignment, and passing or returning structures are unsupported. Pointer
support is limited: pointer-to-pointer types, function pointers,
addresses of automatic locals, whole-array or dynamic-element
addresses, array-to-pointer decay, and pointer arithmetic other than `++`,
`--`, `+=`, and `-=` are unsupported. Multidimensional and variable-length
arrays and non-byte/word array elements are unsupported.
Named structure tags do not yet have C's separate, scoped tag namespace:
later `struct Tag` references are not resolved to an earlier definition.
Keep a structure definition with the declaration that uses its type; do not
rely on forward tag declarations or reuse of a tag across declarations.

Signed relational comparisons, signed right shifts, and signed division and
remainder are unsupported; integer operators otherwise use unsigned semantics.
Signed overflow and uninitialized local reads are not diagnosed. Runtime array
bounds are not checked, so out-of-range runtime accesses have undefined
behavior.

Compound assignment (`+= -= *= /= %= <<= >>= &= ^= |=`) and `++`/`--` work on
variables, array elements, members and dereferences, but the target must not
contain side effects (no calls, assignments or `++` inside the subscript).
Pointer variables accept `++`, `--`, `+=` and `-=`, scaled by the pointee size.

Switches accept unsigned byte and unsigned word controlling expressions and
nonnegative integer constant case labels in the corresponding range. Case
labels fall through in source order; `break` exits the nearest loop or switch,
and `continue` skips intervening switches to target the enclosing loop.
Duplicate case/default labels and labels outside a switch are rejected. Signed
controlling expressions and negative signed case labels remain unsupported until
Task 5.3.9. Dispatch uses PASM `CASE`/`ENDCASE` tables instead of inline
comparison chains: byte controls dispatch directly on A, while word controls
dispatch first on the high byte and then on the low byte. Each table's explicit
PASM `ELSE` entry routes unmatched values to the C default or switch exit
(forward-referenced `ELSE` targets require the corrected `pasm.exe` shipped
with this repository).
PASM's CASE table
stores a one-byte value plus a 16-bit target per entry and keeps the entry
count in the one-byte H register. Each generated table has at most 255 entries;
word switches may contain more than 255 cases as long as there are at most 255
high-byte groups and at most 255 cases in each group.

Global scalar initializers are integer and character constant expressions
using `+ - * / % & | ^ ~ << >> !` and unary `+`/`-` (constant results are
truncated modulo 65536); scalar list and floating-point initializers are
rejected. Array subscripts accept supported integer
expressions, including function calls and other array accesses; each subscript
is evaluated once. Constant out-of-range subscripts are rejected. Runtime
bounds are not checked, and out-of-range runtime access has undefined behavior.
Multidimensional arrays, variable-length arrays, array-to-pointer decay, and
non-byte/word element types remain unsupported.
Array brace lists may be shorter than the declared extent; omitted elements
are zero-initialized, including local arrays. An extent may be inferred from a
nonempty brace list. Comparison, shift, multiply, divide and XOR operations
call runtime routines (`LIB_CP*16`, `LIB_SHL16`/`LIB_SHR16`, `LIB_MUL16`,
`LIB_DIV16`/`LIB_MOD16`, `LIB_XOR16`), embedded once after generated functions
when needed. Their source is built into the compiler, so no external `.lib`
files are required.
Unknown runtime requests and function names colliding with included runtime
labels are rejected before CLI output is written. Negative constants may be
written with unary minus (`-1` is `0xFFFF` in a word).

### Operator semantics

| Operator | Behavior |
|----------|----------|
| `+`, `-`, `*` | Wrap modulo 65536 (`*` keeps the low 16 bits) |
| `/`, `%` | Unsigned; constant zero divisors are rejected. A runtime zero divisor gives quotient `0xFFFF` and remainder equal to the dividend (not standard C; avoid) |
| `&`, `\|`, `^`, `~` | Bitwise on the 16-bit promoted operands |
| unary `-`, `+` | Two's complement negate (`0 - x`) / identity |
| `<<`, `>>` | Logical. A count of 16 or more yields 0; a zero count is the identity. Signed `>>` is rejected |
| `&&`, `\|\|` | Short-circuit; result is 1 or 0 |
| `!` | Result is 1 or 0 |
| `,` | Evaluates left, then right; yields the right value. Void calls are rejected as operands |
| `==`, `!=`, `<`, `<=`, `>`, `>=` | Unsigned; result is 255 for true and 0 for false (differs from the 1 produced by `!`, `&&`, `\|\|`; compare against zero rather than `1`) |

### Types and conversions

Bytes are 8 bits; words are 16 bits. Plain `char` is unsigned. Expressions
use the word accumulator B:A; byte loads zero-extend for unsigned types and
sign-extend for `signed char`. Byte assignments retain the low byte, and
assignment expressions yield the converted stored value. Unsigned word
arithmetic wraps modulo 65536. Integer constants must fit in 16 bits
(0..65535); larger literals are rejected.

Signed overflow and reads of uninitialized locals are not diagnosed.
Signed relational comparisons, signed `>>`, `/` and `%`, and casts remain
outside the implemented subset.

Supported pointers are 16-bit values returned in B:A. Pointer-returning calls
may be stored, forwarded, or dereferenced. Converting to a matching
`const`-qualified pointee is supported; discarding pointee `const` is rejected.

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
initializers are constant expressions (see above); results are truncated to the declared width (8 or 16 bits) and stored
before `main` as part of startup.

Single-dimensional byte and word arrays use the same fixed register/XRAM
placement rules as scalars. Global array initializers require brace lists of
constant expressions; local array initializers accept supported scalar
expressions. Both zero-fill omitted elements; uninitialized local arrays are
not zeroed. Element loads/stores support constant or runtime subscripts.
Word-element address scaling preserves the full 16-bit offset; XRAM arrays
are not restricted to a single 256-byte page. Array extents count toward
storage-overlap checks and the local frame limit.

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
