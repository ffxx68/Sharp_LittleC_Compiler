# Refactor roadmap for `lcc` (SC61860)

This document collects the analysis and the refactor plan to make the `lcc` compiler located in `Source/lcc` more modular, readable and maintainable.

## Executive summary

-   Goal: separate responsibilities (lexing, parsing, symbol table, semantic, codegen, output, optimizations, utilities) into distinct Pascal units, keeping backward compatibility and allowing incremental, verifiable refactors.
-   Approach: incremental refactor in phases, running tests and compilation after each phase, using compatibility wrappers to preserve public APIs.

---

## Initial checklist (to complete)

-   Create a git branch `refactor_modular` (recommended).
-   Run a baseline build (compile `Source/lcc`) and save the output/log.
-   Create this roadmap file (this file).

---

## Build environment notes

-   Lazarus is installed in: `C:UsersF.Fumilazarus`
-   The compiler is: `$(Lazarusdir)fpc3.2.2bini386-win32fpc.exe`
-   Baseline build log: `Source/lcc/baseline_build.log`

---

## Test Scripts

-   **test.bat**: Script avanzato di regression testing.
    
    -   Ricompila automaticamente il nuovo compilatore `lcc` dai sorgenti correnti.
    -   Esegue un confronto (regression test) tra il compilatore originale (`lcc.exe` nella root) e quello nuovo (`Source/lcc/lcc.exe`).
    -   Preprocessa i file con `lcpp.exe`.
    -   Compila i file di test con entrambe le versioni e confronta gli output (`reference.asm` vs `new.asm`) usando `fc /W` (file compare).
    -   Supporta l'esecuzione su una singola demo (passando il nome come argomento) o su una suite predefinita di demo ("16bitdiv", "Array", "bounce", "Loop demo", "Math demo").
    
    **Esempi d'uso:**
    
    -   `test.bat` → Esegue l'intera suite di regressione.
    -   `test.bat bounce` → Esegue il test solo per la demo "bounce" (utile per debug rapido).
    -   `test.bat Array` → Esegue il test solo per la demo "Array" (quella che mostra differenze).

---

## Current component map (high-level)

-   `input.pas`: character input, `GetChar`, global variable `Look`.
-   `scanner.pas`: tokenization (GetToken, GetName, GetNumber, GetFloat), string parsing utilities, input modes (file/string/keyboard), global variables `Tok`, `dummy`, `Level`, `md`, `linecnt`.
-   `parser.pas` / `parser_new.pas`: parsing, symbol table (VarList/ProcList), semantic actions (AddVar/AddProc/AllocVar/repadr), direct asm generation, post-process optimizations.
-   `CodeGen.pas`: label generation, helpers varxram/varcode/varreg, operation helpers (CompEqual, CompGreater, ...), library handling.
-   `output.pas`: accumulation of `asmtext`/`libtext`/`asmlist`, `writeln` wrapper and `addasm` functions.
-   `errors.pas`: Error/Expected that use scanner globals for context.
-   `calcunit.pas`: Evaluate() for evaluating mathematical expressions.

---

## Main issues identified

-   Parsing, semantic analysis and code emission are mixed in the same file (`parser.pas`).
-   Heavy use of global variables shared between units (Look, Tok, dummy, linecnt, pushcnt, optype...).
-   Duplication between `parser.pas` and `parser_new.pas`.
-   Post-processing/optimizations are mixed into `SecondScan`.

---

## Target components (proposed)

For each component I list responsibilities, minimal API, dependencies and priority.

1.  `Lexer` (extracted from `scanner.pas`)

-   Responsibility: lexing/tokenization, string utilities strictly necessary for the lexer.
-   Minimal API: `InitFromFile`, `InitFromString`, `GetToken(mode, var s)`, `GetName`, `GetNumber`, `GetFloat`, `CopyToken`, `CurrentToken`, `CurrentLine`.
-   Dependencies: `Input`, `Errors`.
-   Priority: High.

2.  `SymbolTable` (extracted from parser VarList/ProcList)

-   Responsibility: management of VarList/ProcList, Find/Add/Alloc/IsVarAtAdr/RemoveLocalVars.
-   Minimal API: `FindVar`, `AddVar`, `AllocVar`, `FindProc`, `AddProc`, `RemoveLocalVars`, getters for metadata.
-   Dependencies: `CalcUnit` (for mathparse), `Errors`.
-   Priority: High.

3.  `Semantic` (vardecl / repadr / checks logic)

-   Responsibility: var-declaration parsing helpers, repadr, type/overlap validations.
-   Minimal API: `ParseVarDecl(tok: string): string` (or similar wrapper), `RepAdr(currproc)`.
-   Dependencies: `SymbolTable`, `CalcUnit`, `Errors`.
-   Priority: High.

4.  `CodeGen` (refactor)

-   Responsibility: instruction emission, label management, library handling, varXxx helpers.
-   Minimal API: `NewLabel`, `PostLabel`, `EmitInst`, `AddLib`, `VarXram`, `VarCode`, `VarReg`, `Flush`.
-   Dependencies: `Output`.
-   Priority: High.

5.  `Output` (clean)

-   Responsibility: accumulate `asmtext`/`libtext`/`asmlist`, write to file.
-   Minimal API: `Emit`, `AddAsm`, `SaveToFile`, `Reset`.
-   Priority: High.

6.  `Backend` (optimizations and post-processing)

-   Responsibility: transformations of `asmtext` (pattern replace), instruction compression, steps currently in `SecondScan`.
-   Minimal API: `OptimizeAsm(asmText): string`, `WriteAsmFile(filename, asmText, libText)`.
-   Priority: Medium-High.

7.  `CalcUnit` (clean up)

-   Responsibility: `Evaluate()` for expressions and hex/bin conversions.
-   Minimal API: `Evaluate`, `ConvertHex`, `ConvertBin`.
-   Priority: Medium.

8.  `Errors` (improve)

-   Responsibility: error reporting with context (line, token, dummy)
-   Minimal API: `Error(msg)`, `Expected(msg)`, `Warning(msg)`.
-   Priority: Medium.

---
# Phase 0 — Preparation

-   Task 0.1: create a git branch for the refactor.
-   Task 0.2: run baseline build (compile `Source/lcc`) and save the output.
-   Task 0.3: generate and save reference ASM (copy original `tmp.asm` -> `Source/lcc/reference_bounce.asm`) — used as regression reference for future tests.

---
# Phase 1 — Extract `Lexer`

-   Task 1.1: create `Lexer.pas` that re-exports the public functions from `scanner.pas` (GetToken, GetName...).
-   Task 1.2: replace direct uses in the `parser` with `Lexer` (use wrappers to keep compatibility).

```
-   Done: updated `parser.pas` to call `Lexer.Get*` wrappers where needed and verified by running the `bounce` demo; generated `tmp.asm` matches `reference_bounce.asm` (NO_DIFF).
```

-   Verification: build OK, run tokenization test on a demo file (generated `test_Cbouncetmp.asm` matches `reference_bounce.asm`).

---
# Phase 2 — Extract `SymbolTable` and `Semantic`

## Task 2.1: create `SymbolTable.pas` and move VarList/ProcList, Find/Add/Alloc. (COMPLETED)

```
-    Created initial `Source/lcc/SymbolTable.pas` (minimal stub) and then a fuller implementation with Var/Proc API (FindVar, AddVar, AllocVar, GetVar/GetProc accessors).
-    Added overloaded compatibility API `IsVarAtAdr(adr,size; out foundIdx)` plus a two-arg wrapper for backwards compatibility.
-    Replaced caller sites in `Source/lcc/parser.pas` for variable-address queries to use `SymbolTable.IsVarAtAdr` (incremental, targeted replacements only — kept backup of original file).
-    Added `Source/lcc/symboltest.pas` and verified it compiles (`SYMBOLTEST_OK`).
-    Ran full demo test (build + lcpp + lcc) for `test_C/bounce` — result: `NO_DIFF` (no regression on generated asm).
-    Migrated `FindVar` and `FindProc` to use `SymbolTable` API (wrapper functions).
-    Added synchronization in `AddVar` and `AddProc` to keep `SymbolTable` in sync with local arrays.
-    Created conversion functions `VarEntryToTVarInfo` and `ProcEntryToTProcInfo` for type compatibility.
-    Migrated read accesses to `VarList[i]`/`ProcList[i]` to use `GetVarInfo(i)`/`GetProcInfo(i)` in print functions and initialization loops.
-    Migrated `VarList[VarFound]`/`ProcList[ProcFound]` accesses to use `GetVarInfo(varfound)`/`GetProcInfo(procfound)`.
-    Verified all changes with `test.bat` — result: `NO_DIFF` (no regression).
-   Note: Local arrays `VarList`, `ProcList`, `VarCount`, `ProcCount` still exist in `parser.pas` for write operations during parsing. These will be fully removed in a future refactor phase.
```

##  Task 2.2: create `Semantic.pas` for vardecl/repadr/checks. (COMPLETED)

```
-    Created `Source/lcc/Semantic.pas` with `VarDecl` and `RepAdr` functions.
-    Added `SetVarAddress(idx, newAddress)` to `SymbolTable.pas` API for updating variable addresses.
-    Migrated `RepAdr` function to use `SymbolTable` API (`GetProcInfo`, `FindVar`, `SetVarAddress`).
-    Updated `parser.pas` to delegate to `Semantic.RepAdr` and sync back to local arrays for compatibility.
-    Verified with `test.bat` — result: `NO_DIFF` (no regression).
```

-   Verification: `FirstScan` produces var/proc tables identical to the baseline.

---
# Phase 3 — Refactor `CodeGen` + `Output`

## Task 3.1: add `EmitInst` in CodeGen and use `Output.Emit` instead of direct `writeln`. (COMPLETE)

```
-    Created `EmitInst` family of functions in `CodeGen.pas`:
    -   `EmitInst(inst)` - single instruction
    -   `EmitInst(inst, operand)` - instruction with operand
    -   `EmitInst(inst, operand, comment)` - instruction with operand and comment
    -   `EmitComment(comment)` - standalone comment
    -   `EmitBlankLine` - blank line
-    **parser.pas migrations** (~300+ writln/writeln calls converted):
    -   Store: ALL variants (byte/char/word/float + arrays, register/XRAM, local/global)
    -   LoadConstant, LoadVariable: ALL data types, modes, and arrays
    -   Factor: address-of operator (&), procedure calls, pointer dereferencing
    -   Assignment: ALL assignment types including pointer assignments
    -   Control flow: DoIf, DoWhile, DoLoop, DoFor, DoDoWhile, Switch, DoGoto, DoBreak, DoReturn
    -   DoLoad, DoSave: Added missing procedures
    -   ProcCall: COMPLETE (parameters, locals, stack management) - migrated to CodeGen.EmitComment/EmitBlankLine
    -   Block: procedure returns, inline assembly (#asm blocks) - migrated end blank line to CodeGen.EmitBlankLine
    -   DoLabel: migrated to PostLabel + CodeGen.EmitComment
    -   Increment/decrement operators (++/--)
-    **CodeGen.pas partial migrations** (~30 writln calls converted):
    -   load_x, load_y helpers
    -   varxram, varxarr (byte/char small arrays)
-    **SecondScan fixes**: ALL `writeln()` now write to file with `writeln(f, ...)` instead of console
    -   Fixed intro section, registry save/restore
    -   Fixed procedure loop output
    -   Fixed asmlist output
-    **Console output cleanup**: NO assembly code on console anymore! ✅
-    All tests passing with **NO_DIFF** - zero regressions!
-    **Status: COMPLETE**:
    -   All `writln` in parser.pas migrated to CodeGen API
    -   Only 1 intentional `writln` remains in parser.pas (line ~2467): inline assembly (#asm blocks) - writes directly to asmtext buffer
    -   Remaining ~150 `writln` in CodeGen.pas deferred to incremental future refactoring (still functional)
```

## Task 3.2: consolidate `addlib` and `libtext` handling into `CodeGen` -> `Output`. (COMPLETE)

```
-    Created library text management API in `Output.pas`:
    -   `AddLibText(s: string)` - appends text to libtext buffer
    -   `GetLibText(): string` - returns libtext content
    -   `ClearLibText()` - clears libtext buffer
-    Modified `CodeGen.pas` `addlib()` to use `AddLibText()` instead of direct `libtext :=` assignment
-    Modified `parser.pas` `SecondScan` to use `GetLibText()` instead of direct `libtext` access
-    Fixed UTF-8 BOM issue in parser.pas that was introduced during editing
-    Verified with `test.bat` — result: **NO_DIFF** (no regression)
```

-   Verification: `SecondScan` generates functional asm; build OK. ✅
-   **Phase 3 COMPLETE** ✅

---
# Phase 4 — Reduce Parser: separate syntax from emission

##  Task 4.1: Create high-level CodeGen functions to replace repetitive EmitInst blocks in parser.pas (~341 calls)

```
**Step 4.1.1 — Store functions for global variables in registers** ✅ Create in `CodeGen.pas`:

-    `StoreByteToReg(adr: integer; name: string)` — LP/LIP (adr<64?) + EXAM
-    `StoreWordToReg(adr: integer; name: string)` — LP/LIP + EXAM + EXAB + INCP + EXAM
-    Update `StoreVariable` in parser.pas to use queste funzioni
-    Verify: `test.bat` → NO_DIFF
-    Also added: `NewLabel`, `PostLabel`, `load_x`, `CompGreater`, `CompSmaller` to CodeGen interface

**Step 4.1.2 — Store functions for local variables (stack-based)** ✅ Create in `CodeGen.pas`:

-    `StoreByteToLocal(adr, pushcnt: integer; name: string)` — EXAB + LDR + ADIA(adr+2+pushcnt) + STP + EXAB + EXAM
-    `StoreWordToLocal(adr: integer; name: string)` — PUSH + LDR + ADIA + STP + POP + EXAM + EXAB + DECP + EXAM
-    Update `StoreVariable` in parser.pas
-    Verify: NO_DIFF

**Step 4.1.3 — Store functions for XRAM variables** ✅ Create in `CodeGen.pas`:

-    `StoreByteToXram` — LIDP + STD (adr=-1 → use name)
-    `StoreWordToXram` — LIDP + STD + LIDL/LIDP + STD
-    Update `StoreVariable` in parser.pas
-    Verify: NO_DIFF

**Step 4.1.4 — Load functions for global variables in registers** ✅ Create in `CodeGen.pas`:

-    `LoadByteFromReg` — LP/LIP + LDM
-    `LoadWordFromReg` — LP/LIP(adr+1) + LDM + EXAB + DECP + LDM
-    Update `LoadVariable` in parser.pas
-    Verify: NO_DIFF
```

**Step 4.1.5 — Load functions for local variables** ✅ Create in `CodeGen.pas`:

-   `LoadByteFromLocal` — LDR + ADIA(adr+2+pushcnt) + STP + LDM
-   `LoadWordFromLocal` — LDR + ADIA + STP + LDM + EXAB + INCP + LDM
-   Update `LoadVariable` in parser.pas
-   Verify: NO_DIFF
-   Note: `pushcnt` is global in CodeGen.pas, removed from function parameters

**Step 4.1.6 — Load functions for XRAM variables** ✅ Create in `CodeGen.pas`:

-   `LoadByteFromXram` — LIDP + LDD
-   `LoadWordFromXram` — LIDP(adr+1) + LDD + EXAB + LIDL/LIDP + LDD
-   Update `LoadVariable` in parser.pas
-   Verify: NO_DIFF

**Step 4.1.7 — Array element access functions (byte)** ✅ Create in `CodeGen.pas`:

-   `LoadArrayByteFromReg`
-   `LoadArrayByteFromXram`
-   `StoreArrayByteToReg`
-   `StoreArrayByteToXram`
-   Update `StoreVariable` and `LoadVariable` array sections
-   Verify: NO_DIFF

**Step 4.1.8 — Array element access functions (word)** ✅ Create in `CodeGen.pas`:

-   `LoadArrayWordFromReg`
-   `LoadArrayWordFromXram`
-   `StoreArrayWordToReg`
-   `StoreArrayWordToXram`
-   Update parser.pas
-   Verify: NO_DIFF

**Step 4.1.9 — Code formatting consistency fix** ✅

-   Added `EmitInstComment(inst, comment)` to CodeGen.pas for instructions without operand
-   Replaced 18 occurrences of `EmitInst(inst, '', comment)` with `EmitInstComment(inst, comment)`
-   Fixed TAB/SPACE formatting inconsistencies in generated assembly
-   Regenerated reference_bounce.asm with consistent formatting
-   Verified float constant generation (NO differences found with backup/parser.pas)
-   Verify: NO_DIFF ✅

**Step 4.1.9b — Regression fixes (Array demo & others)** ✅

-   Run full regression suite using `test.bat` (covers 16bitdiv, Array, bounce, Loop demo, Math demo)
-   **Fixed regressions in "Array" demo**:

```
-   Issue 1: Byte values > 127 corrupted during array initialization (232 → 63) due to FPC 3.x UTF-8 codepage conversion
-   Solution: Used `{$H-}` + SetLength + direct chr() assignment instead of string concatenation
-   Issue 2: Missing `SREG: .DW 0, 0, 0, 0, 0, 0` label in output
-   Solution: Fixed typo `writeln` → `writln` at line 2489 to write to file instead of console
```

-   **Fixed regressions in "Loop demo" and "Math demo"**:

```
-   Issue: Missing shift operator debug comments `; <<` and `; >>`
-   Solution: Fixed typo `writeln` → `writln` at lines 1337 and 1339
```

-   Verify: `test.bat Array` → **NO_DIFF** ✅
-   Verify: `test.bat bounce` → **NO_DIFF** ✅
-   Verify: `test.bat "Loop demo"` → **NO_DIFF** ✅
-   Verify: `test.bat "Math demo"` → **NO_DIFF** ✅
-   **Full regression suite: 4/4 working tests PASS** ✅

```
-   Note: 16bitdiv excluded (missing getkey.h dependency)
```

-   Enhanced `test.bat` with summary report showing test counts and detailed results

**Step 4.1.10 — Pointer dereferencing functions** ✅

-   `LoadPointerContentXram(pnttyp: string; name: string)` — LP 4 + EXAM + LP 5 + EXAB + EXAM + DX + IXL (word: +EXAB+IXL+EXAB)
-   `LoadPointerContentReg(pnttyp: string; name: string)` — STP + LDM (word: +EXAB+INCP+LDM+EXAB)
-   `LoadAddressOf(adr: integer; name: string; isXram: boolean)` — LIA LB + LIB HB (xram) or LIA adr (reg)
-   Update `Factor` procedure in parser.pas
-   Verify: NO_DIFF ✅

**Step 4.1.11 — Fix "Possible Stack corruption!" warning** ✅

-   Root cause identified: duplicate POP in Assignment procedure for array element access
-   In `Assignment` (parser.pas), lines 1724-1727 had an extra `POP ; pop array index` that was already handled inside `StoreVariable`
-   Fix: Removed the duplicate POP block since `StoreVariable` already manages the array index POP internally
-   Also added missing `LoadVariable` support for word array XRAM access (lines 1177-1221)
-   Verified: 16bitdiv demo now compiles without "Possible Stack corruption!" warning
-   Note: Array demo still has a separate issue with word array XRAM expression parsing (not stack-related)
-   `lcc.exe` in root updated with fix
-   Regression validation: 16bitdiv passes ✅

**Step 4.1.12 — Stack management functions** ✅

-   Created in `CodeGen.pas`:
    -   `AllocStackSpace(bytes: integer)` — if <8: PUSH×n, else: LP 0 + EXAM + LDR + SBIA + STR + EXAM
    -   `FreeStackSpace(bytes: integer; hasReturn, isWord: boolean)` — various patterns based on return type (isWord → LP/EXAM/LDR/ADIA/STR/EXAM; hasReturn → EXAB/LDR/ADIA/STR/EXAB; no return → POP×n or LDR/ADIA/STR)
-   Updated `ProcCall` in parser.pas: replaced all raw writln emission blocks with `AllocStackSpace`, `FreeStackSpace`, `EmitInst`, `EmitComment`, `EmitBlankLine`
-   Fixed `EmitInst(inst, operand, comment)` to use space (not tab) when operand is empty, matching reference format
-   Fixed all `EmitComment` calls that incorrectly included a leading `; ` prefix (double semicolon issue)
-   Note: `pushcnt` is global in CodeGen.pas — not passed as parameter
-   Verify: `test.bat` → **NO_DIFF** (all 6 demos: 16bitdiv, Array, bounce, Loop, Math, pointer) ✅

**Step 4.1.13 — Increment/Decrement register functions** Create in `CodeGen.pas`:

-   `EmitIncReg(regAddr: integer)` — INCI/INCJ/INCA/INCB/INCK/INCL/INCM/INCN/INCP based on addr (0,1,2,3,8,9,10,11,12)
-   `EmitDecReg(regAddr: integer)` — DECI/DECJ/DECA/DECB/DECK/DECL/DECM/DECN/DECP
-   Update increment/decrement instructions in parser.pas
-   Verify: `test.bat` → **NO_DIFF**  ✅

**Step 4.1.14 — Verify and remove residual ASM emission in parser.pas**

**Implementation notes:**

-   Functions that modify stack must handle `pushcnt` as `var` parameter
    
-   When `adr = -1`, use symbolic name instead of numeric address
    
-   When `adr < 64`, use `LP`; otherwise use `LIP`
    
-   All changes must be verified incrementally with `test.bat` → NO_DIFF against `reference_bounce.asm`
    


---

## Task 4.3: Complete the migration of the ~385 residual `writln` in parser.pas

**Current status:** ~385 active `writln` in `parser.pas` + 8 in `CodeGen.pas`.
These are concentrated in:
-   `StoreVariable` (~80 writtn): store byte/word/float for register, local, xram, array
-   `LoadVariable` (~60 writln): loading variables
-   `Assignment` (~40 writln): optimized increment/decrement (INC*/DEC*)
-   Control flow: `DoIf`, `DoWhile`, `DoFor`, `DoLoop`, `DoDoWhile`, `Switch`, `DoReturn`, `DoBreak`, `DoGoto`, `DoLabel` (~60 writln)
-   `Expression` (~5 writln): shift operator comments
-   `ProcCall`, `Block`, `SecondScan` (~140 writln): procedure call, prologue/epilogue, final output

**Goal:** eliminate direct `writln` in `parser.pas`, using only `CodeGen.EmitInst`/`EmitComment`/`PostLabel` and high-level CodeGen functions.

**Prerequisite for porting**: as long as the parser emits inline ASM, it's not possible to separate the frontend from the backend.

-   Verification: `test.bat` → NO_DIFF



---
## Task 4.4: Fixing compiler issues

This task addresses preexisting compiler bugs discovered during refactoring.

*Possibly not required if embracing the Flex/Bison rewrite (Phases 5 on)*

**Step 4.4.1 — Wrong array init code**  ✅

**Problem description:**

The array definition is malformed in the generated asm.

**Symptoms:**

E.g. this code:
```
byte xram bXSnake[4];
```
generates this
```
bYSnake:	; Variable bYSnake = (0, 0, 0, 0)
0, 0, 0, 0
```
instead of
```
bXSnake:	; Variable bXSnake = (0, 0, 0, 0)
.DB 0, 0, 0, 0
```

**Root cause analysis:**

Bug in CodeGen.pas, procedure `varcarr`:
```pascal
if (typ = 'char') or (typ = 'char') or (typ = 'float') then
```
fixed to
```pascal
if (typ = 'byte') or (typ = 'char') or (typ = 'float') then
```

**Step 4.4.2 — Fix word array XRAM expression parsing**

**Problem description:**

In the Array demo, the expression `a[0] = b[0] + e[1]` (where `e` is `word xram e[100]`) does not generate correct code for `e[1]`.

**Symptoms:**

-   The compiler generates `LIA e [1] ; Load byte constant e [1]` treating the entire `e[1]` as a literal string constant
-   No array access code is generated for `e[1]` (no RC, SL, address calculation, IXL sequence)
-   The addition `+ e[1]` is completely missing from the output

**Root cause analysis:**

1.  In `Assignment` procedure, `forml` contains the right-hand side expression `b[0]+e[1]`
2.  The `find_text()` check at lines 1683-1690 finds `e` in `forml` and sets `fv := true`
3.  However, `Expression` is then called with the current `Look`/`tok` state which may have been modified
4.  The `Factor` procedure (lines 1375-1383) should detect array access when `Look = '['`, but this detection fails for the second term in an addition
5.  When `Add` procedure calls `Term` → `Factor`, the parser state may not correctly position `Look` at the start of `e[1]`

**Files involved:**

-   `parser.pas`: `Assignment`, `Expression`, `Add`, `Term`, `Factor` procedures
-   `scanner.pas`: `GetName`, token handling

**Proposed fix approach:**

1.  Add debug logging to trace parser state during expression evaluation
2.  Verify that after `Rd(Look, tok)` in `Add`, the `Look` character is correctly positioned at `e`
3.  Check if `GetName` correctly extracts `e` and leaves `Look` at `[`
4.  Ensure `LoadVariable` for word array XRAM (lines 1177-1221) is being reached

**Test case:**

```c
#org 33000
char a[6], b[7];
word xram e[100];
main() {
    a[0] = b[0] + e[1];
}
```

**Expected assembly output (partial):**

```asm
; Load b[0]
LIB     14      ; array b address
LP      3
ADM
EXAB
STP
LDM             ; b[0] value in A

PUSH            ; save b[0] for addition

; Load e[1] - word array XRAM
LIA     1       ; index
RC
SL              ; index*2 for word array
PUSH
LP      5       ; HB of address
LIA     HB(e-1)
EXAM
LP      4       ; LB
LIA     LB(e-1)
EXAM
POP
LIB     0
ADB
DX
IXL             ; HB of e[1]
EXAB
IXL             ; LB of e[1]
EXAB

; PopAdd - add b[0] + e[1]
...
```


**Step 4.4.3 — Complete fix to "Possible Stack corruption!" warning**

**Problem description:**

The "Possible Stack corruption!" warning is caused by unbalanced PUSH/POP instructions.
Fix 4.1.1 possibily incomplete.

**Symptoms:**

The warning appears during compilation of the Array demo, for example.

---
# Phase 5 — Introduction of AST and frontend/backend separation

Motivation: the current parser is a syntax-directed translator — it emits code during parsing. To replace the frontend with a standard C frontend (Flex/Bison) we need an intermediate AST that completely decouples parsing from codegen.

> Change of direction compared to littleC: the goal is NOT to replicate littleC's idiosyncrasies (non-standard operator precedence, `loop`, `switch` with custom semantics, etc.) but to converge toward a subset of the C89/C99 standard, within the limits of the SC61860 platform. Existing littleC programs may require minimal adjustments to compile with the new frontend.

## Task 5.1: Define the target C subset

Write `Docs/sc61860_c_subset.md`: the specification of the language accepted by the new compiler.

Supported standard C constructs (target):

| Area | Supported | Platform notes |
|------|-----------|----------------|
| Types `char`, `unsigned char` | ✅ | 8 bit, always unsigned on SC61860 |
| Types `int`, `unsigned int` | ✅ | 16 bit (= littleC `word`) |
| Type `long` | ❌ | 32 bit not practical (too slow, little RAM) |
| Type `float` | ✅ | Sharp BCD, 8 bytes, via library |
| `void` | ✅ | For functions without return (littleC forbids this — to be fixed) |
| `const` | ✅ | Useful for code-space data |
| `signed`/`unsigned` | ✅ | littleC has no signed — to be added |
| Pointers | ✅ | 16 bit; pointers as parameters (littleC does not support) |
| One-dimensional arrays | ✅ | Max 256 bytes / 128 words (HW limit) |
| `struct` | ✅ | Fundamental for idiomatic C code |
| `union` | ⚠️ | Possible but low priority |
| `enum` | ✅ | Simple: integer constants |
| `typedef` | ✅ | Type aliases, no runtime cost |
| `if`/`else` | ✅ | Standard |
| `while`, `do..while` | ✅ | Standard |
| `for` (3 expressions) | ✅ | Standard (littleC executes step before body — bug) |
| `switch`/`case`/`break`/`default` | ✅ | Standard C semantics (littleC has different semantics) |
| `break`, `continue` | ✅ | `continue` is missing in littleC |
| `return` | ✅ | Standard |
| `goto`/label | ✅ | Standard C label (`label:` not `label name;`) |
| Ternary operator `?:` | ✅ | Missing in littleC |
| Compound assignment `+= -= ...` | ✅ | Already in littleC |
| `++`/`--` prefix and postfix | ✅ | littleC has only postfix |
| Standard C operator precedence | ✅ | littleC has inverted precedence — to be fixed |
| Preprocessor `#include #define #ifdef #else #endif` | ✅ | Handled by `lcpp`, add `#else` |
| `#pragma org(addr)` | ✅ | Portable substitute for `#org` |
| Inline assembly `__asm { ... }` | ✅ | Substitute for `#asm..#endasm`, more standard syntax |

Deprecated littleC constructs (not carried forward):

| littleC construct | C standard substitute |
|-------------------|----------------------|
| `loop(n) { ... }` | `for (int i=0; i<=n; i++) { ... }` |
| `switch` with `value: proc()` | `switch(x) { case V: proc(); break; }` |
| `label name;` | `name:` (C standard label) |
| `byte`/`word`/`char` as type keywords | `unsigned char` / `unsigned int` / `char` (with convenience typedef) |
| `at` / `xram` in declarators | `__at(addr)` / `__xram` attribute (or `#pragma`) |
| `#org`, `#pc`, `#nosave` | `#pragma org(addr)`, `#pragma pc(addr)`, `#pragma nosave` |
| Names that cannot start with a type | No restriction (parser fix) |

Platform-specific extensions (attributes starting with `__`):

```c
// Variable allocated to a fixed CPU register address
unsigned char __at(8) pbuf;

// Variable in external RAM (XRAM)
__xram unsigned int baspnt;
__xram __at(0xFFD7) unsigned int baspnt;  // with address

// Inline assembly
__asm {
    LIA  0x42
    LP   8
    EXAM
}

// Configuration pragmas
#pragma org(33000)
#pragma nosave
```

## Task 5.2: Define AST nodes (standard C)

Create `ast.h` with nodes that reflect the C grammar, not littleC's grammar:

| Node | Main fields |
|------|-------------|
| `TranslationUnit` | `decls[]` (top-level declarations) |
| `VarDecl` | `type`, `name`, `init`, `attrs` (`__at`, `__xram`) |
| `FuncDecl` | `retType`, `name`, `params[]`, `body` |
| `ParamDecl` | `type`, `name` |
| `StructDecl` | `tag`, `members[]` |
| `EnumDecl` | `tag`, `values[]` |
| `TypedefDecl` | `type`, `alias` |
| `CompoundStmt` | `items[]` (decls + stmts, like C99) |
| `IfStmt` | `cond`, `then`, `else` |
| `WhileStmt` | `cond`, `body` |
| `DoWhileStmt` | `body`, `cond` |
| `ForStmt` | `init`, `cond`, `step`, `body` |
| `SwitchStmt` | `expr`, `body` (with `CaseLabel`/`DefaultLabel`) |
| `ReturnStmt` | `expr` (optional) |
| `BreakStmt` | — |
| `ContinueStmt` | — |
| `GotoStmt` | `label` |
| `LabelStmt` | `name`, `stmt` |
| `ExprStmt` | `expr` |
| `AsmStmt` | `code` |
| `BinaryExpr` | `op`, `left`, `right` |
| `UnaryExpr` | `op`, `operand`, `prefix` |
| `TernaryExpr` | `cond`, `then`, `else` |
| `CastExpr` | `type`, `expr` |
| `CallExpr` | `func`, `args[]` |
| `IndexExpr` | `array`, `index` |
| `MemberExpr` | `obj`, `field`, `isArrow` |
| `IdentExpr` | `name` |
| `ConstExpr` | `value`, `type` |
| `SizeofExpr` | `type` or `expr` |
| `AddrOfExpr` | `operand` |
| `DerefExpr` | `operand` |

## Task 5.3: Document formal BNF grammar

Write `Docs/sc61860_c_grammar.y` (directly in Bison-compatible format):

- Start from simplified C89 grammar (e.g. K&R Appendix A or `c99.y` by Jutta Degener).
- Remove unsupported constructs (`long`, `double`, `volatile`, `register`, multi-dimensional arrays, function pointers, variadic `...`).
- Add platform extensions (`__at`, `__xram`, `__asm`).
- Use **standard C** operator precedence — not littleC's precedence.

## Task 5.4: Prototype AST in Pascal (validation)

- Implement `AST.pas` with the records/classes of Task 5.2.
- Modify a single parser procedure (e.g. `DoIf`) to produce an AST node + a tree-walker that emits the same code.
- Verification: `test.bat` → NO_DIFF on at least the `If` test.
- Purpose: validate that the AST → codegen architecture can produce identical output before rewriting everything in C.

---
# Phase 6 — Backend: extract peephole optimizer

## Task 6.1: Extract optimizations from `SecondScan` into `Backend.pas`

The current optimizations in `SecondScan` (lines ~2800-3020 of `parser.pas`) are textual passes on temporary files:

| Step | Pattern → Replacement |
|------|-----------------------|
| 1 | `EXAB; EXAB` → remove |
| 2 | `LIA x; EXAB` → `LIB x` |
| 3 | `PUSH; LIB x; POP` → `LIB x` |
| 4 | `LIB 1; LP 3; ADM; EXAB` → `INCA` |
| 5 | `LIB 1; LP 3; EXAB; SBM; EXAB` → `DECA` |

Extract into testable functions: `OptimizeDoubleEXAB()`, `OptimizeLIAtoLIB()`, `OptimizeRedundantPUSHPOP()`, `OptimizeIncDec()`.

## Task 6.2: Unit tests for each peephole optimization pattern

- Verification: `test.bat` → NO_DIFF

---
# Phase 7 — New C compiler in Flex/Bison

Prerequisites: BNF grammar (Task 5.3), AST validated (Task 5.4), backend decoupled, peephole extracted.

> From this phase the work is on a new C codebase (`Source/C/lcc2/`), parallel to the Pascal one. The Pascal compiler stays functional as reference for regression tests.

## Task 7.1: Project setup in C

- Create `Source/C/lcc2/` with structure:
    ```
    lcc2/
    ├── Makefile          (or CMakeLists.txt)
    ├── ast.h             (AST nodes from Task 5.2)
    ├── ast.c             (node allocators/destructors)
    ├── lexer.l           (Flex)
    ├── parser.y          (Bison)
    ├── symtab.h / .c     (symbol table)
    ├── codegen.h / .c    (SC61860 backend)
    ├── peephole.h / .c   (optimizations)
    ├── output.h / .c     (ASM accumulation and writing)
    ├── platform.h        (platform constants: register addresses, stack layout)
    └── main.c
    ```
- Toolchain: `gcc` (MinGW on Windows), `flex`, `bison`
- Build: `make` produces `lcc2.exe`

## Task 7.2: Flex lexer (`lexer.l`)

C standard tokens + platform extensions:

```
/* keywords C */
auto break case char const continue default do else enum
extern float for goto if int long register return short
signed sizeof static struct switch typedef union unsigned void volatile while

/* platform keywords */
__at __xram __asm

/* pragma */
#pragma

/* multi-char operators */
++ -- << >> <= >= == != && || += -= *= /= %= <<= >>= &= |= ^= -> ?:

/* literals */
INTEGER_CONST    [0-9]+ | 0x[0-9a-fA-F]+ | 0b[01]+
FLOAT_CONST      [0-9]+\.[0-9]*
CHAR_CONST       '.'  | '\n' | '\0' etc.
STRING_CONST     "..."
IDENTIFIER       [a-zA-Z_][a-zA-Z0-9_]*
```

Keywords that are not supported (`long`, `volatile`, `register`, etc.) are recognized by the lexer but generate a semantic error in the parser ("unsupported on this platform").

## Task 7.3: Bison parser (`parser.y`)

Start from simplified C89 grammar. Semantic actions build the AST (`ast.h`).

Precedence rules — **standard C**:
```bison
%right '=' ASSIGN_OP
%right '?' ':'
%left  OR_OP           /* || */
%left  AND_OP          /* && */
%left  '|' 
%left  '^'
%left  '&'
%left  EQ_OP NE_OP
%left  '<' '>' LE_OP GE_OP
%left  SHL_OP SHR_OP
%left  '+' '-'
%left  '*' '/' '%'
%right UNARY            /* ! ~ ++ -- * & (type) sizeof */
%left  POSTFIX          /* () [] . -> ++ -- */
```

Main productions:
- `translation_unit → external_declaration*`
- `external_declaration → function_definition | declaration`
- `declaration → type_specifier declarator_list ';'`
- `function_definition → type_specifier declarator compound_statement`
- `compound_statement → '{' block_item* '}'`
- `block_item → declaration | statement`
- `statement → if | while | do_while | for | switch | return | break | continue | goto | labeled | expr_stmt | compound | asm_stmt`

Platform extensions handled as attributes:
```bison
attribute_specifier: __AT '(' constant_expression ')'
                   | __XRAM
                   ;
```

## Task 7.4: SC61860 backend — tree-walker (`codegen.c`)

Translate `CodeGen.pas` (~1900 lines) into C. The API is already stabilized by Phase 4.1.x:

```c
// Emission
void emit_inst(const char *inst);
void emit_inst_op(const char *inst, const char *operand);
void emit_inst_op_comment(const char *inst, const char *operand, const char *comment);
void emit_comment(const char *comment);
void emit_label(const char *label);
char *new_label(void);

// Store/Load (already defined in Phase 4)
void store_byte_to_reg(int addr, const char *name);
void store_word_to_reg(int addr, const char *name);
void store_byte_to_local(int addr, const char *name);
// ... etc.

// Entry point: walks the AST
void codegen(TranslationUnit *tu);
```

What's new compared to the littleC codegen:
- `signed` arithmetic: need `SBB` (subtract with borrow) and signed comparisons
- `struct`: access via offsets computed by the compiler
- `switch/case`: jump table or chain of comparisons (more efficient than old littleC `ENDCASE`)
- `continue`: jump to the increment step in `for`, or to the condition in `while`
- Standard `for`: step executed **after** the body (fix to littleC bug)

## Task 7.5: Symbol table (`symtab.c`)

- Nested scopes (scope stack: global → function → block) — required for standard C
- Lookup with shadowing (local variable hides global)
- `struct` tag namespace separate
- Allocator for registers/XRAM/stack with the same rules as littleC (bottom-up for registers, top-down for stack)

## Task 7.6: Peephole optimizer (`peephole.c`)

Port from `Backend.pas` (Task 6.1). Same patterns, but operate on an in-memory array of instructions (not on temporary files).

## Task 7.7: Regression and compatibility tests

Strategy with two levels:

1. Compatibility tests for littleC: compile existing demos (with minimal syntax changes: `byte` → `unsigned char`, etc.) and compare ASM to references. Output will not be byte-for-byte identical (operator precedence differences, correct `for`, etc.) but must be functionally equivalent.

2. Tests for new constructs: write specific tests for:
    - `void f(void) { ... }`
    - `switch/case/default/break`
    - `continue`
    - `struct point { int x; int y; }; struct point p; p.x = 10;`
    - `enum color { RED, GREEN, BLUE };`
    - `typedef unsigned char byte;`
    - `int *p = &x; *p = 42;` (pointers as params)
    - `signed int` comparisons and arithmetic
    - Ternary operator `a > b ? a : b`

## Task 7.8: Migrate library headers

- Rewrite `lcd.h`, `key.h`, `puts.h`, `int2str.h`, `string.h` in standard C syntax
- Add minimal `<stdint.h>`: `typedef unsigned char uint8_t; typedef unsigned int uint16_t;`
- Provide `compat.h` with convenience typedefs: `typedef unsigned char byte; typedef unsigned int word;`

---
# Phase 8 — Advanced optimizations (post-porting)

With an AST available, implement optimizations that the old syntax-directed translator could not do:

- Task 8.1: Constant folding — evaluate constant expressions at compile time
- Task 8.2: Dead code elimination — remove `if(0)` branches, unused functions
- Task 8.3: Register allocation — smarter use of registers I, J, K, L, M, N
- Task 8.4: Strength reduction — `x * 2` → `x << 1`, `x * 3` → `(x << 1) + x`
- Task 8.5: Tail call optimization — useful given the very small stack (~80 bytes)
- Task 8.6: Inline expansion — inline small functions (1–3 instructions)

---
# Phase 9 — Cleanup, docs and release

- Task 9.1: Remove Pascal codebase (or archive in `Source/legacy/`).
- Task 9.2: Write the user manual for the new compiler (`Docs/sc61860cc_manual.md`).
- Task 9.3: Update README with build instructions (`make` / `cmake`).
- Task 9.4: CI scripts (GitHub Actions or similar) for automated regression tests.
- Task 9.5: Publish release with prebuilt binaries (Windows, Linux).
- Verification: build + full test suite green on Windows and Linux.

---

# Riepilogo fasi e dipendenze

```
Phase 0-3  ✅  Completate (modularizzazione Pascal)
Phase 4    🔧  In corso (separazione sintassi ↔ emissione)
  └─ Task 4.3: eliminare tutti i writln residui in parser.pas

Phase 5    📋  AST + specifica linguaggio C target
  ├─ 5.1: specifica sottoinsieme C per SC61860
  ├─ 5.2: definire nodi AST (C standard)
  ├─ 5.3: grammatica BNF formale (formato Bison)
  └─ 5.4: prototipo AST in Pascal (validazione)

Phase 6    📋  Estrazione peephole optimizer

Phase 7    📋  Nuovo compilatore Flex/Bison (C) ← core del porting
  ├─ 7.1: setup progetto C
  ├─ 7.2: lexer Flex
  ├─ 7.3: parser Bison
  ├─ 7.4: backend SC61860 tree-walker
  ├─ 7.5: symbol table con scope annidati
  ├─ 7.6: peephole optimizer
  ├─ 7.7: regression test + test nuovi costrutti
  └─ 7.8: migrazione header libreria

Phase 8    📋  Ottimizzazioni avanzate (AST-based)
Phase 9    📋  Cleanup e rilascio
```

**Dipendenze critiche:**
```
Task 4.3 ──→ Phase 5 ──→ Phase 6 ──→ Phase 7
                │                        │
                └── 5.3 (BNF) ──────────→ 7.3 (Bison)
                └── 5.2 (AST def) ──────→ 7.4 (codegen)
```

---

# Main risks and mitigations

| # | Risk | Impact | Mitigation |
|---|------|--------|------------|
| 1 | ~385 `writln` residui bloccano la separazione frontend/backend | Alto | Completare Task 4.3 prima di tutto |
| 2 | Programmi littleC esistenti non compilano col nuovo frontend C | Medio | Fornire `compat.h` con typedef e documentare guida di migrazione |
| 3 | Divergenza output ASM tra compilatore Pascal e compilatore C | Medio | Test funzionali (non byte-per-byte) + esecuzione su emulatore |
| 4 | `struct` richiede calcolo offset e gestione alignment | Medio | SC61860 non ha requisiti di alignment — semplifica |
| 5 | `signed` arithmetic richiede nuove librerie ASM | Medio | Implementare `cmp_signed16.lib`, `neg8.lib`, `neg16.lib` |
| 6 | Peephole fragile se il formato dell'output cambia | Basso | Operare su array di istruzioni strutturate, non su testo |
| 7 | Flex/Bison non disponibili facilmente su Windows | Basso | `winflexbison` (pacchetto precompilato), oppure build in WSL |

---

# Practical development suggestions

-   Completare Phase 4 (zero `writln` nel parser) è il **prerequisito critico**.
-   Phase 5 (AST) si può prototipare in Pascal — serve solo a validare che il disaccoppiamento funziona. Non serve portare l'intero parser: basta un costrutto (`DoIf`).
-   Per Phase 7, partire da un **micro-compilatore C esistente** come reference architetturale:
    -   **chibicc** (Rui Ueyama): ~5000 righe, ottima documentazione, C11 quasi completo, molto didattico.
    -   **8cc** (stesso autore): predecessore di chibicc, più piccolo.
    -   **lcc** (Fraser & Hanson): compilatore C accademico con backend retargetable — il più vicino concettualmente.
    -   **cproc** (Michael Forney): ~6000 righe, C11, backend QBE — buon esempio di struttura.
-   Usare `test.bat` per le fasi Pascal; per le fasi C, creare un `Makefile` con target `test` equivalente.
-   I typedef di convenienza (`byte`, `word`) in `compat.h` permettono di compilare gran parte dei demo esistenti con modifiche minime.

---

# Quality checklist (Quality Gates)

**Fasi 4-6 (Pascal):**
-   Build: FPC compila senza errori.
-   Regression: `test.bat` → NO_DIFF su tutte le demo.
-   Nessuna emissione ASM diretta nel parser (da Phase 5 in poi).

**Fasi 7+ (C):**
-   Build: `make` compila senza errori né warning (`-Wall -Wextra`).
-   Test funzionali: ogni demo produce ASM assemblabile con `pasm` e funzionalmente corretto.
-   Test nuovi costrutti: `struct`, `enum`, `switch/case`, `continue`, `signed`, `void`, ternario.
-   Nessun memory leak (valgrind o AddressSanitizer).
-   Grammatica Bison: zero conflitti shift/reduce e reduce/reduce.

---

# Acceptance criteria (target finale)

1.  Il compilatore `lcc2` (C, Flex/Bison) accetta un **sottoinsieme significativo di C89** con estensioni piattaforma.
2.  L'output ASM è assemblabile con `pasm` e funzionalmente corretto su SC61860.
3.  I demo littleC esistenti compilano con modifiche minime (via `compat.h`).
4.  Costrutti nuovi (`struct`, `enum`, `switch/case`, `signed`, `void`, `continue`, ternario) funzionano.
5.  Il frontend è una grammatica Bison leggibile, senza conflitti.
6.  Il backend SC61860 è un modulo C separato, riusabile con altri frontend.
7.  Il peephole optimizer opera su strutture dati (non su testo) ed è testabile unitariamente.
8.  Documentazione completa: specifica linguaggio, grammatica, manuale utente, istruzioni di build.

---