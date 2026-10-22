# SC61860 C Language Subset

This document specifies the target language for the new SC61860 compiler
described in Phase 5 of `Source/lcc/refactor_roadmap.md`. It is a design
contract for that compiler, not a claim that the existing Pascal `lcc`
already accepts every feature below. The goal is a useful C89 subset with
selected C99 conveniences and explicit SC61860 extensions.

Unless stated otherwise, constructs follow C semantics: standard operator
precedence and associativity, left-to-right execution of statements, and the
usual C rules for `if`, loops, `switch`, `break`, `continue`, and `return`.
The target has limited memory and no operating-system runtime; generated
programs run directly on the SC61860.

## Types and data model

| Type or feature | Status | Target contract |
|---|---|---|
| `char`, `unsigned char` | Supported | 8-bit byte; plain `char` is unsigned. |
| `signed char` | Supported | 8-bit signed integer. |
| `int`, `unsigned int` | Supported | 16-bit integer, corresponding to littleC `word`. |
| `short`, `long`, `long long` | Not supported | Integer types wider than the target's supported byte and word types are out of scope. |
| `float` | Supported | 8-byte Sharp BCD value, implemented using the target's floating-point routines/libraries. |
| `double`, `long double` | Not supported | No wider floating-point representation is provided. |
| `void` | Supported | Function return type for procedures that do not return a value; also usable as `void *`. |
| `const` | Supported | Qualifies an object as non-modifiable through that lvalue. |
| `volatile` | Planned, not required initially | Needed only where a hardware or externally changed object requires it. |
| Pointers | Supported | 16-bit addresses. Object pointers, pointer parameters, dereference, address-of, and pointer arithmetic are in scope. |
| One-dimensional arrays | Supported | Fixed-size arrays only. Total storage for one array may not exceed 256 bytes (256 byte elements or 128 word elements). |
| `struct` | Supported | Members are stored in declaration order; no bit-fields. |
| `union` | Optional / low priority | May be deferred; if implemented, all members share storage starting at offset zero and the size is that of the largest member. |
| `enum` | Supported | Enumerators are integer constants; enum objects use the target's 16-bit integer representation. |
| `typedef` | Supported | Introduces a type alias and does not allocate storage. |

Integer arithmetic uses the width of the selected type. Unsigned arithmetic
wraps modulo the type's range. Signed values use the corresponding 8-bit or
16-bit two's-complement range; signed overflow is undefined. Integer
conversions and promotions follow C rules to the extent supported by these
target types. A conversion that cannot be represented in the destination
signed type has undefined behavior.

Pointers are addresses in the target's 16-bit address space. Pointer
arithmetic advances by the size of the pointed-to object, and is defined only
within an object or one position past its end. Dereferencing an invalid,
one-past-the-end, or otherwise inaccessible address is undefined. Function
pointers and pointer-to-pointer type combinations are not required in the
initial subset.

Array bounds are compile-time constants. Multi-dimensional arrays, variable
length arrays, dynamic allocation, and arrays whose total byte size exceeds
256 bytes are not supported. The limit applies to the complete array object,
including arrays of structures.

## Declarations and functions

The supported declaration forms include global and block-scope objects,
function definitions and prototypes, parameters by value, pointers and
fixed-size arrays. Objects may be initialized with supported constant
expressions; aggregate initialization is limited to forms the implementation
can lay out statically.

The initial grammar follows C89 declaration placement: declarations precede
statements within a block. C99 mixed declarations and statements are not
required. Recursion is not guaranteed because execution uses a small
hardware-targeted stack; programs must keep stack usage within the
application's reserved memory.

`struct` members are laid out in declaration order without bit-fields.
Structure size is the sum of member sizes; padding and alignment are not
inserted by the initial target ABI. Structure assignment, parameter passing,
and return-by-value are not required initially; structures can be declared
and accessed through objects and pointers. A structure or union object,
including an array element, remains subject to the array/storage limits above.

## Statements and expressions

The compiler supports:

- Compound statements, expression statements, and empty statements.
- `if` / `else`, `while`, `do ... while`, and `for` with its three optional
  expressions and standard execution order.
- `switch`, `case`, `default`, and `break`, with standard C fall-through
  semantics. Case labels are integer constant expressions and unique within
  their switch.
- `continue` in loops, `return`, `goto`, and labels written `name:`.
- Function calls and the supported unary, arithmetic, relational, equality,
  bitwise, logical, assignment, and comma operators.
- Prefix and postfix `++` and `--`, compound assignment, and the conditional
  operator `?:`.

Expressions use standard C precedence and associativity. The `for` step
expression is evaluated after each body execution, including after a
`continue`; it is not evaluated before the first body execution. `break`
exits the innermost loop or switch, while `continue` applies to the innermost
loop. A non-void function must return a value on every path where its value
is used; a void function returns without a value.

The initial subset does not require floating-point remainder, bitwise
operations on floating-point values, function pointers, or implementation-
specific integer widths.

## Preprocessing

Preprocessing is performed by `lcpp` before parsing. The supported target
directives are:

- `#include`, `#define`, `#ifdef`, `#ifndef`, `#else`, `#endif`, and
  `#undef`.
- `#pragma org(address)`, `#pragma pc(address)`, and `#pragma nosave`.

Preprocessor behavior should follow C conventions for macro expansion and
conditional inclusion within the features supported by `lcpp`. Function-like
macros are desirable but are not a prerequisite for the first compiler
milestone. `#if` / `#elif`, token pasting, and stringification are not
required initially.

## SC61860-specific extensions

The following extensions use reserved names beginning with `__`:

```c
/* Allocate an object at a fixed SC61860 register or memory address. */
unsigned char __at(8) pbuf;

/* Place an object in external RAM. */
__xram unsigned int baspnt;
__xram __at(0xFFD7) unsigned int fixed_baspnt;

/* Emit assembler instructions directly. */
__asm {
    LIA  0x42
    LP   8
    EXAM
}
```

`__at(address)` applies to an object declaration and reserves the specified
address; invalid or overlapping placements are diagnostics. `__xram` selects
the target's external-RAM addressing behavior. Inline assembly is emitted
verbatim as assembler source; C names and operands inside the block are not
substituted. The compiler does not validate register use, clobbers, or
instruction semantics in inline assembly.

The configuration pragmas are written as:

```c
#pragma org(33000)
#pragma pc(33000)
#pragma nosave
```

The argument to `org` and `pc` is an integer constant expression. The exact
effect of each pragma is target configuration: `org` selects the program's
origin, `pc` sets the initial program-counter setting where supported, and
`nosave` disables the compiler-generated register save/restore wrapper.

## Compatibility and diagnostics

These littleC constructs are not part of the new language:

| Legacy construct | Use instead |
|---|---|
| `loop(n) { ... }` | A standard `for` or `while` loop (declare a `for` counter before the loop in the initial C89 subset). |
| `switch` arms written as `value: proc()` | `switch (x) { case value: proc(); break; }`. |
| `label name;` | A C label, `name:`. |
| `byte` / `word` type keywords | `unsigned char` / `unsigned int`, or project typedefs. |
| `at` / `xram` declarator keywords | `__at(address)` / `__xram`. |
| `#org`, `#pc`, `#nosave` | The corresponding `#pragma` forms. |

Unsupported syntax, invalid type combinations, duplicate case labels, and
out-of-range or conflicting fixed-address declarations should produce a
clear compile-time diagnostic rather than silently generating incorrect
assembly. Undefined C behavior is not required to be detected.

## Implementation milestones

This is the intended language contract, not a release-by-release feature
claim. Each feature becomes available only when its lexer/parser support,
semantic checks, backend implementation, and tests are complete. The Phase
5.3 regression and functional tests in the roadmap track that incremental
implementation.
