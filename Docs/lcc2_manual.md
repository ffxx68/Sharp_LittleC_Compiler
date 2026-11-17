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

| Area | Support |
|---|---|
| Types | 8-bit `char`, `unsigned char`, `signed char`; 16-bit `int`, `unsigned int`, `signed int`; `void` returns and `const` objects |
| Functions | Prototypes and definitions, direct calls, recursion, byte/word and supported pointer parameters and returns; `main` must take no parameters |
| Data | Fixed-address global scalars and single-dimensional byte/word arrays; automatic local variables and arrays; brace-list initialization |
| Structures and pointers | Byte/word members; byte/word pointers to supported objects, including assignment, dereference, function parameters/returns, and `->` access |
| Statements | Blocks, `if`/`else`, loops, `switch`, `break`, `continue`, `return`, and labels |
| Expressions | Constants, calls, array subscripts, assignment, arithmetic, bitwise, shifts, logical and comparison operators, unary operators, and comma |
| Extensions | Translation-unit typedefs, `__at`, `__xram`, `#pragma org`, and `#pragma nosave` |

This is a limited C subset, not a full C89 implementation. Preprocess source
separately when using headers. Declarations must precede statements in a
block. Function parameters are byte/word scalars or supported pointers;
prototypes may omit parameter names. Calls evaluate arguments left to right.
Non-void functions need a return expression.

### Limitations

`short`, `long`, floating point, `volatile`, `static` and `extern` storage,
block-scoped typedefs, `enum`, `union`, `goto`, casts, `sizeof`, the
conditional `?:` operator, and string literals are unsupported. Declared
external function calls are accepted, but only supplied library modules are
included automatically; there is no general external-object linker.

Structures cannot contain arrays, pointers, or nested structures, and cannot
be initialized, assigned, passed, or returned as whole structures. Pointers
to pointers and function pointers are unsupported, as are addresses of local
variables, whole arrays, dynamic array elements, and array-to-pointer decay.
Pointer arithmetic is limited to `++`, `--`, `+=`, and `-=`. Arrays must be
one-dimensional, with byte or word elements and a fixed extent; an extent can
be inferred from a nonempty initializer.

Signed integer operations are not fully implemented. Relational comparisons,
right shifts, division, and remainder on signed values are rejected unless
the compiler can prove the operands nonnegative. Other integer operations
use unsigned semantics. Integer constants must fit in 16 bits. Signed
overflow and uninitialized local reads are not diagnosed.

Array bounds are not checked at runtime; out-of-range accesses have undefined
behavior. Constant out-of-range subscripts are rejected. Compound assignment
and increment/decrement are supported on variables, array elements, members,
and dereferences, but their target expressions must not have side effects.
`switch` uses unsigned byte or word controls and nonnegative constant case
labels. Cases fall through in source order; `break` exits the nearest loop or
switch, and `continue` targets the nearest enclosing loop.

### Types, initialization, and storage

Bytes are 8 bits and words are 16 bits. Plain `char` is unsigned; byte values
are extended according to their type. Byte assignments keep the low byte.
Unsigned arithmetic wraps modulo 65536. Logical operators return 0 or 1;
comparisons return 0 or 255, so test comparison results against zero rather
than against 1.

Global variables require explicit addresses. Internal-RAM objects must fit
within addresses 8..95; `__xram` objects use external RAM. Objects in the
same address space must not overlap. Global scalar initializers must be
constant expressions. Array initializers use brace lists; omitted elements
are zero-initialized. Uninitialized local arrays are not cleared.

Placement is the programmer's responsibility: keep code, data, and the
runtime stack clear of each other and of BASIC/firmware memory. The local
stack frame is limited to 32 bytes, and available stack space also depends on
parameters, nested calls, and expression temporaries. Use reserved external
RAM for results that must remain visible after returning to BASIC.

### Pragmas

| Pragma | Behavior |
|---|---|
| `#pragma org(address)` | Sets the assembly origin; the default is `0xE030`. |
| `#pragma nosave` | Disables register preservation in the startup wrapper. |
| `#pragma pc(address)` | Not supported. |
