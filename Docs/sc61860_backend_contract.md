# SC61860 Backend Compatibility Contract

This document records the Pascal backend (`Source/lcc/CodeGen.pas`) as the
behavioral reference for the staged C backend. It is intentionally an
operation-level contract, not a promise that every legacy helper or legacy
language behavior will be ported. The C implementation must pass state
explicitly and should port only the operations needed by each frontend
milestone.

## Context and invariants

The Pascal implementation is a stateful unit. Its public operations read or
mutate the following state; the C backend must own equivalent state in an
explicit `CodegenContext`.

| Pascal state | Meaning | C backend invariant |
|---|---|---|
| `pushcnt` | Number of bytes currently reserved/occupied on the evaluation/call stack; helpers increment/decrement it as they emit stack operations. Local accesses calculate offsets using its current value. | Keep byte-accurate stack depth in the context. Every emitted push/pop or bulk allocation/restoration updates it exactly once; diagnose underflow or imbalance at function boundaries. |
| `optype` | Selects byte, word, or float behavior for `Push`, `Pop*`, comparisons, and unary operations. | Pass value type/width explicitly to each operation; do not share mutable process-global type state. |
| `LCount` | Monotonic counter used by `NewLabel`, producing `LB0`, `LB1`, etc. | Keep a translation-unit label counter in the context; labels must be unique and deterministically allocated in emission order. |
| `libins`, `libname`, `libcnt` | Tracks and deduplicates required runtime routines. | Track library use per output unit and append resolved routine definitions after optimized generated assembly. |
| `Output.asmtext`, `asmcnt` / `asmlist`, `outfile` | `writLn` appends to the main assembly buffer when `outfile` is true; otherwise it stores a line in `asmlist`. | Provide explicit output destinations/buffers. Preserve main assembly, separately buffered data, and their final ordering. |
| `Output.libtext` | Accumulates library directives/data independently. | Keep resolved runtime definitions separate from code/data until final serialization. |

`EmitPush` / `EmitPop` and many `Pop*`, comparison, and storage helpers
manipulate `pushcnt` internally. The C API must either preserve that ownership
rule or centralize all stack mutations in wrappers, but must not count a
single operation twice.

## Operation inventory and selected initial API

| Semantic group | Pascal public operations | Inputs / state read | Emission and state effects | C port priority |
|---|---|---|---|---|
| Instruction and comment emission | `EmitInst` overloads, `EmitInstComment`, `EmitComment`, `EmitBlankLine`, `EmitRawAsmLine`, `EmitRawAsmBlock` | Instruction, optional operand/comment/text; reads output destination | Emits tab-prefixed instruction/operand/comment lines, a tab-prefixed comment, blank line, or raw line; `EmitRawAsmBlock` emits each body line with a leading tab. No stack/library changes. | First; centralize output formatting and routing. |
| Stack primitives | `EmitPush`, `EmitPop`, `Push` | Optional comment; `Push` reads `optype` and float register layout | `EmitPush/Pop` emit `PUSH`/`POP` and change `pushcnt` by ±1. `Push` emits one byte push, two pushes for a word (A then B with `EXAB`), or an eight-byte float-copy loop from `FloatXReg` using a fresh label; it increments depth for each pushed byte. | First; byte/word initially, float when float expressions are implemented. |
| Labels and procedures | `NewLabel`, `PostLabel`, `PostUserLabel`, `PostProcedureLabel`, `EmitSkippedProcedureComment` | Label/procedure name; reads and changes `LCount` only in `NewLabel` | `NewLabel` returns `LB<n>` and increments the counter. Posting helpers emit the label/comment line with established formatting. No library/stack effects. | First; deterministic labels are required for branches and loops. |
| Constants and scalar access | `LoadByteConstant`, `LoadWordConstant`; `LoadByte/WordFromReg/Local/Xram`; `StoreByte/WordToReg/Local/Xram` | Value/width; symbol name; register address or `-1` for symbolic XRAM; local byte offset; local helpers read `pushcnt`. | Byte constant: `LIA value`; word constant: load `LB(value)` into A and `HB(value)` into B. Register accesses use `LP` below address 64, otherwise `LIP`; XRAM uses `LIDP` with `LDD`/`STD`. Word operations transfer both bytes with `EXAB` and `INCP`/`DECP`. Local operations derive P from R with `LDR`, `ADIA`, `STP`. No library includes. | First; byte/word globals and locals form the scalar subset. |
| Arrays and pointers | `Load/StoreArrayByte/WordFrom/ToReg/Xram`; `Load/StorePointerContentReg/Xram`; `LoadAddressOf`; `load_x`, `load_y` | Element base/address, symbol, element width, index already in registers/stack, pointee type, storage class. Array/pointer operations may adjust `pushcnt`. | Register-array addressing adds the index to base; word arrays double the index (`SL`). XRAM arrays form the base in X/Y registers and use `IXL`/`IYS`. Pointer helpers use X for XRAM loads and Y for XRAM stores; word dereference transfers both bytes. Address-of loads low/high address bytes for XRAM, low byte for register-space values. No library includes. | After scalar milestone; validate separately by storage/element width. |
| Arithmetic, comparisons, unary | `PopAdd/Sub/Mul/Div/Mod/And/Or/Xor/SL/SR`, `CompEqual/NotEqual/Smaller/SmOrEq/Greater/GrOrEq`, `Negate`, `NotIt` | Operand type via `optype`, stack layout, and primary/TOS values; helpers consume stack bytes. | Byte add/sub/bitwise operations use SC61860 instructions; word add/sub/bitwise operations manipulate A:B and stack bytes. Byte/word multiply, divide, shifts, word modulo/XOR and word comparisons call library labels and mark required libraries. Float add/sub/multiply/divide use fixed ROM entry calls and float registers; several float bitwise/shift/modulo operations are explicitly unsupported. `NotIt` emits inline instructions. See library mapping below. | Port only as needed; operation contracts must be type-explicit. Signed arithmetic/comparison is not provided by this legacy API. |
| Branch and control flow | `BranchAbs`, `Branch`, `BranchAbsFalse`, `BranchFalse`; `EmitSwitch*`, `BeginSwitchArm`, `EndSwitchArm` | Label/value; conditions in A; switch helpers also mutate output destination | Unconditional forms emit `JP` or `RJMP`. False branches emit `TSIA 255` then `JRZP` / `JPZ`, followed by a blank line. Switch helpers emit the legacy `CASE`/`ENDCASE` dialect and temporarily route arm lines to `asmlist`. No library changes. | Branch/labels first; use the legacy switch behavior only where it satisfies the documented C semantics, otherwise implement C switch independently. |
| Calls and stack frames | `AllocStackSpace`, `FreeStackSpace`, `EmitProcCall` | Byte counts, return kind/width, local comments; reads/writes `pushcnt` | Allocation below 8 bytes emits repeated `PUSH`; at 8+ uses `LP 0`, `EXAM`, `LDR`, `SBIA n`, `STR`, `EXAM`. Freeing a byte/word result restores R while preserving the result; void cleanup below 4 bytes uses repeated `POP`, otherwise adjusts R with `LDR`/`ADIA`/`STR`. `EmitProcCall` emits optional local allocation, `CALL name`, and optional argument cleanup. No library changes. | First procedure/call milestone; test thresholds and all return modes. |
| Startup and initialized data | `EmitProgramStartup`, `EmitGlobalVariableInitialization`, `varxram`, `varxarr`, `varcode`, `varfcode`, `varcarr`, `varfarr`, `varreg`, `varrarr`, `FinalizeAndWrite` | Origin/save flag; name/type/address/size/value/storage/initializer data; output filename | Startup emits headers, `.ORG`, optional SREG save/restore wrapper and `CALL main`. Initialization dispatches by register/XRAM, fixed-address, scalar/array, and float status; some declarations go through the separately buffered `asmlist`. Final output writes assembly and library buffers. | Needed for complete programs; preserve ordering and distinct data/code routing. |
| Register increment/decrement | `EmitIncReg`, `EmitDecReg` | Register address and optional comment | Emits the corresponding `INC*`/`DEC*` instruction; unsupported register addresses fall back to `INCA`/`DECA`. No state/library changes. | Optional helper; callers should validate addresses instead of depending on fallback. |

## Runtime-library contract

`addlib(id)` records each required routine once. Before output is written,
`ResolveRuntimeLibraries` searches the working directory and executable
directory and their parents for each mapped `.lib`, verifies that it defines
the expected `LIB_*` label, and appends its source to the separate library
buffer. Missing or malformed helpers raise an explicit error rather than
leaving an external call in generated output. Runtime source is appended after
optimization so optimizer rewrites cannot alter library calling conventions.
These are the mappings declared in `CodeGen.pas`; only routines actually
called by the selected operations should be included.

| Library id | Runtime source | Selected operation use |
|---|---|---|
| `MUL8` | `mul8.lib` | Byte multiplication |
| `DIVMOD8` | `divmod8.lib` | Byte division and modulo |
| `SR8`, `SL8` | `sr8.lib`, `sl8.lib` | Byte shifts |
| `XOR8` | `xor8.lib` | Byte XOR; also requested by word XOR |
| `MUL16`, `DIV16`, `MOD16` | `mul16.lib`, `div16.lib`, `mod16.lib` | Word multiply, divide, modulo |
| `AND16`, `OR16` | `and16.lib`, `or16.lib` | Declared library ids; current `PopAnd/PopOr` emit inline instructions. |
| `XOR16`, `SR16`, `SL16` | `xor16.lib`, `sr16.lib`, `sl16.lib` | Word XOR and shifts |
| `NOT16`, `NEG16` | `not16.lib`, `neg16.lib` | Declared ids; current `NotIt/Negate` emit inline sequences. |
| `CPE16`, `CPNE16`, `CPS16`, `CPG16`, `CPSE16`, `CPGE16` | Correspondingly named `.lib` files | Word equality, inequality, less-than, greater-than, less-or-equal, greater-or-equal comparisons. |

Float arithmetic currently calls ROM addresses directly (`0x10C2`, `0x10D8`,
`0x10E1`, `0x10EA` in `CodeGen.pas`) rather than requesting an include. These
entry points are model-specific and must not be represented as portable
SC61860 library calls without separate target validation.

## Initial C backend milestone contract

The first C vertical slice should support only byte/word constants, supported
scalar storage classes, expression stack push/pop, byte/word add/subtract,
equality and relational branch generation, labels, simple functions/calls,
and program startup/output. Every operation receives a `CodegenContext *`
plus explicit width, storage/address, and symbol inputs. The context owns
output buffers, library-use flags, label allocation, stack depth, and
function-local emission state. Do not port Pascal global variables or parser
tables into the C backend.

Add arrays, pointers, floats, and the remaining arithmetic/library operations
as separately validated milestones. Signed arithmetic and standard-C switch
semantics require new implementations; the legacy routines are unsigned and
the legacy switch helpers emit a separate assembler dialect.

## C backend behavior

### C array slice (Task 5.3.4.8)

The C implementation uses the established storage conventions and extends
subscripts to full 16-bit integer expressions and supports local arrays.
Constant subscripts resolve to scalar locations; dynamic subscripts compute
an address once, with 16-bit word scaling, then use P for register/local RAM
or X/Y with `IXL`/`IYS` for XRAM. Local words retain reversed stack byte order.
Stores preserve the converted assignment value without reevaluating the
index, including when the index or RHS calls a function.

`tests/test_codegen.c` covers addressing and stack effects, and
`tests/arrays.c`, `tests/arrays_wide.c`, and
`tests/arrays.lua` supply functional gates across storage classes,
widths, page crossings, and byte offsets of 256. Unsupported array forms are
diagnosed; dynamic out-of-bounds accesses retain C's undefined behavior.

### Legacy reference fixture inventory

The existing `Source/lcc/test_C` inputs document legacy littleC examples and
are historical material, not a feature inventory or acceptance oracle for
`lcc2`. The supported C subset and its tests define the acceptance contract.

| Behavior under contract | Fixture |
|---|---|
| Byte/word scalar access, constants, and arithmetic | `test_C/LoadVariable/main.c`, `test_C/Math/main.c`, `test_C/16bitdiv/main.c` |
| Register/XRAM arrays, byte/word elements, initialization | `test_C/Array/main.c` |
| Pointer address, dereference, and indirect stores | `test_C/pointer/main.c`, `test_C/PointerAccess/main.c` |
| Branches, loops, and switch output | `test_C/Loop/main.c`, `test_C/bounce/main.c`, `test_C/SwitchCase/main.c`, `test_C/SwitchCaseElse/main.c` |
| Float constants and arithmetic | `test_C/Float/main.c` |
| Calls, arguments, local storage, return values, stack thresholds | `test_C/Parameter_passing/main.c`, `test_C/ProcCallStack/main.c` |

For `lcc2`, validate supported constructs against the documented C semantics
and test assembly/runtime behavior at the appropriate level. Where littleC
and standard C differ (notably precedence, `for`, and `switch`), preserve and
test the C behavior functionally.

## Reference locations

The public API and state declarations are in `Source/lcc/CodeGen.pas:13-184`.
Emission/label helpers are at `CodeGen.pas:203-414`, scalar and aggregate
access at `426-986`, library tracking and initialization at `1036-1440`,
comparisons/branches at `1441-1693`, arithmetic at `1694-2243`, and stack
management/calls at `2244-2340`. Buffer routing is defined in
`Source/lcc/Output.pas:13-68`.
