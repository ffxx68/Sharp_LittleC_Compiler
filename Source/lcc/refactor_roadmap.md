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

## Concrete ordered tasks (incremental iterations)

Each item is designed to be small and verifiable.

Phase 0 — Preparation (0.5 day)

-   Task 0.1: create a git branch for the refactor.
-   Task 0.2: run baseline build (compile `Source/lcc`) and save the output.
-   Task 0.3: generate and save reference ASM (copy original `tmp.asm` -> `Source/lcc/reference_bounce.asm`) — used as regression reference for future tests.

Phase 1 — Extract `Lexer` (2 days)

-   Task 1.1: create `Lexer.pas` that re-exports the public functions from `scanner.pas` (GetToken, GetName...).
-   Task 1.2: replace direct uses in the `parser` with `Lexer` (use wrappers to keep compatibility).

```
-   Done: updated `parser.pas` to call `Lexer.Get*` wrappers where needed and verified by running the `bounce` demo; generated `tmp.asm` matches `reference_bounce.asm` (NO_DIFF).
```

-   Verification: build OK, run tokenization test on a demo file (generated `test_Cbouncetmp.asm` matches `reference_bounce.asm`).

Phase 2 — Extract `SymbolTable` and `Semantic` (3 days)

-   Task 2.1: create `SymbolTable.pas` and move VarList/ProcList, Find/Add/Alloc. (COMPLETED)

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

-   Task 2.2: create `Semantic.pas` for vardecl/repadr/checks. (COMPLETED)

```
-    Created `Source/lcc/Semantic.pas` with `VarDecl` and `RepAdr` functions.
-    Added `SetVarAddress(idx, newAddress)` to `SymbolTable.pas` API for updating variable addresses.
-    Migrated `RepAdr` function to use `SymbolTable` API (`GetProcInfo`, `FindVar`, `SetVarAddress`).
-    Updated `parser.pas` to delegate to `Semantic.RepAdr` and sync back to local arrays for compatibility.
-    Verified with `test.bat` — result: `NO_DIFF` (no regression).
```

-   Verification: `FirstScan` produces var/proc tables identical to the baseline.

Phase 3 — Refactor `CodeGen` + `Output` (3 days)

-   Task 3.1: add `EmitInst` in CodeGen and use `Output.Emit` instead of direct `writeln`. (COMPLETE)

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

-   Task 3.2: consolidate `addlib` and `libtext` handling into `CodeGen` -> `Output`. (COMPLETE)

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

Phase 4 — Reduce Parser: separate syntax from emission (4-6 days)

-   Task 4.1: Create high-level CodeGen functions to replace repetitive EmitInst blocks in parser.pas (~341 calls)

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
-   Verify: `test.bat` → **NO_DIFF** ✅ (all 6 demos: 16bitdiv, Array, bounce, Loop, Math, pointer)

**Step 4.1.13 — Increment/Decrement register functions** Create in `CodeGen.pas`:

-   `EmitIncReg(regAddr: integer)` — INCI/INCJ/INCA/INCB/INCK/INCL/INCM/INCN based on addr (0,1,2,3,8,9,10,11)
-   `EmitDecReg(regAddr: integer)` — DECI/DECJ/DECA/DECB/DECK/DECL/DECM/DECN
-   Update `Assignment` increment/decrement section in parser.pas
-   Verify: NO_DIFF

**Step 4.1.14 — Verify and remove residual ASM emission in parser.pas**

**Implementation notes:**

-   Functions that modify stack must handle `pushcnt` as `var` parameter
    
-   When `adr = -1`, use symbolic name instead of numeric address
    
-   When `adr < 64`, use `LP`; otherwise use `LIP`
    
-   All changes must be verified incrementally with `test.bat` → NO_DIFF against `reference_bounce.asm`
    

---

### Task 4.2: Fixing compiler issues

This task addresses preexisting compiler bugs discovered during refactoring.

**Step 4.2.1 — Fix word array XRAM expression parsing**

**Problem description:** In the Array demo, the expression `a[0] = b[0] + e[1]` (where `e` is `word xram e[100]`) does not generate correct code for `e[1]`.

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

**Verification:** `test.bat Array` → NO_DIFF after fix

---

-   Task 4.3: consider introducing an AST (optional) for complex expressions.
    
-   Verification: generated asm is semantically identical - Verify: NO_DIFF
    

Phase 5 — Backend and optimizations (2-3 days)

-   Task 5.1: extract optimization logic (temp -> temp2 passes) into `Backend.pas`.
-   Task 5.2: create tests for optimizations (input with known pattern => expected output).
-   Verification: optimizations preserve equivalence and do not introduce regressions.

Phase 6 — Hardening, cleanup, docs and tests (2 days)

-   Task 6.1: remove duplicates (`parser` vs `parser_new`) after verification and consolidation.
-   Task 6.2: update README, add small scripts/CI for automated tests.
-   Verification: build + smoke tests green.

Estimated total: ~14-18 working days.

---

## Main risks and mitigations

-   Risk: API breakage because many functions rely on globals — Mitigation: create identical wrappers and move functionality progressively.
-   Risk: Differences between `parser.pas` and `parser_new.pas` — Mitigation: compare outputs using a test suite and choose the best base.
-   Risk: optimizations rely on textual matching and are sensitive to formatting/whitespace — Mitigation: implement documented, testable transformation functions.

---

## Practical development suggestions

-   Make frequent, small commits for each extraction ("extract Lexer", "extract SymbolTable").
-   Add a small harness that runs `lcc` over a list of demos and compares the generated asm with reference files (regression tests).
-   Use wrappers to preserve existing public calls (e.g., `GetToken`) and then deprecate the old files gradually.

---

## Quality checklist (Quality Gates)

-   Build: compile the whole `Source/lcc` after each phase — PASS.
-   Lint/Typecheck: check warnings and resolve them — preferably PASS with no critical warnings.
-   Unit tests: add tests for `Lexer`, `SymbolTable`, `Backend.OptimizeAsm` — PASS.
-   Smoke tests: run `lcc` on 3-5 demos and check the asm is assemblable — PASS.

---

## Acceptance criteria

-   All new units compile without errors with FPC/Delphi.
-   The external behavior of `lcc` on reference demos remains equivalent (assembler output is assemblable with `pasm`).
-   The `parser` no longer emits asm strings directly but calls `CodeGen`/`Output` (intermediate target).
-   Documentation updated (`Source/lcc/README.md` or this `refactor_roadmap.md`).

---

## Quick stub unit examples (first-step implementations)

`Lexer.pas` (suggested interface):

```pascal
unit Lexer;
interface
uses SysUtils, Input, Errors;
procedure InitLexerFromFile(const filename: string);
procedure InitLexerFromString(const s: string);
procedure GetToken(mode: Integer; var s: string);
function GetName: string;
function GetNumber: string;
function GetFloat: string;
function CurrentToken: string;
function CurrentLine: Integer;
implementation
// ...existing code... (migrated from scanner.pas) ...
end.
```

`SymbolTable.pas` (suggested interface):

```pascal
unit SymbolTable;
interface
type TVarInfo = record
  Name: string;
  Typ: string;
  Address: Integer;
  Size: Integer;
  Xram: Boolean;
end;
function FindVar(const nm: string; out idx: Integer): Boolean;
function AddVar(const info: TVarInfo): Integer;
function AllocVar(xr, at: Boolean; size, adr: Integer): Integer;
implementation
// ...existing code... (migrated from parser.pas VarList/ProcList) ...
end.
```

---

## Recommended next steps (immediately)

1.  Confirm I can create the branch and start implementing `Lexer.pas` as a wrapper of `scanner.pas` (I can run and compile afterwards).
2.  Alternatively, I can immediately generate the stub files (`Lexer.pas` and `SymbolTable.pas`) and perform a minimal compilation test.

---

File created automatically: `Source/lcc/refactor_roadmap.md`.

---