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

##  Task 4.1: ✅ Create high-level CodeGen functions to replace repetitive EmitInst blocks in parser.pas (~341 calls)

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

Objective: remove the remaining textual ASM emission in `parser.pas` (direct calls to `writeln`/`writln`), moving code generation into centralized APIs (`CodeGen.pas` / `Output.pas`) while keeping functional compatibility (aim: NO_DIFF on the reference demos).

Checklist (short verifiable steps; full `test.bat` after each step: NO_DIFF):

1. ✅ Review `Source/lcc/CodeGen.pas` for existing API stubs for emission: `EmitInst`, `EmitLabel`, `EmitComment`, etc.

2. ✅ Implement `EmitPush`/`EmitPop` that update internal `pushcnt` and emit `PUSH`/`POP` with identical formatting to today.

3. ✅ Migrate `StoreVariable` to `StoreByte`/`StoreWord`/`StoreFloat`/`StoreArray` APIs in `CodeGen`.

   3.1. ✅ Test program 'Float' compilation error (also in reference lcc) — investigate and fix   
 
   3.2. ✅ Still missing "StoreArrayWordToReg/Xram" for word arrays (parser.pas:749 on) — to be implemented.

4. ✅ Replace the emission sites in `parser.pas` for `LoadConstant` and `LoadVariable` with `CodeGen` calls.

   - All constant and variable loads delegate ASM emission to `CodeGen`, including floats; numeric/BCD conversion and type checks remain in the parser.
   - Helpers and `LoadVariable` are aligned with `main`; added `Demos/LoadVariable/main.c` to `test.bat` for global word and register/XRAM array loads.
   - Verification: all 8 demos are NO_DIFF; 144 variable-load and 12 constant comparisons preserve instructions, BCD data, labels and stack accounting.

5. Centralize `ProcCall` logic (reserve locals, push parameters, CALL, FreeStackSpace).

   5.1. ✅ Move local stack reservation, `CALL` emission and stack restoration into a high-level `CodeGen.EmitProcCall`;
           keep argument parsing and pushes in `Parser`.
   - Verification: `test.bat` — all 8 demos NO_DIFF.

   5.2. ✅ Consolidate parameter/local type-size calculations and validate argument-count bounds before indexing procedure metadata.
   - Verification: `test.bat` — all 8 demos NO_DIFF; excess-argument call rejected with the expected diagnostic.

   5.3. ✅ Verify nested calls, byte/word/float parameters, locals, byte/word return values and stack allocation/restoration thresholds; 
            add focused coverage if the current demos do not exercise these cases.
   - Added `Demos/ProcCallStack/main.c` and included it with `Parameter_passing` in `test.bat`. The focused fixture covers 3/4-byte cleanup, 7/8-byte local allocation, byte/word returns, float arguments, and calls within procedures; `Parameter_passing` covers nested argument calls.
   - Verification: `test.bat` — all 10 demos NO_DIFF; generated assembly checked for the stack-threshold instruction sequences.
   - Note: both the reference and refactored compilers print `main: Possible Stack corruption!` for the float-argument case; their generated assembly is identical.

6. Move intro, SREG save/restore, `CALL main` and variable initializations from `SecondScan` into `CodeGen`/`Output`.

   Run `test.bat` after each substep and require NO_DIFF before proceeding.
   Startup emission is straightforward; variable initialization is the main effort because it covers register/XRAM storage, 
   scalars, arrays, and floats. Keep `VarEntry` in `Parser`: pass the required fields to CodeGen APIs rather than introducing a Parser dependency or moving the type.

   6.1. ✅ Capture the current generated assembly for all existing demos as the baseline. Record startup and initialization output ordering, 
           including lines buffered through `addasm`.
   - Regenerated outputs for all 10 demos in an isolated session directory; each matches its existing `test_C/<demo>/new.asm` byte-for-byte.
   - Observed order: intro/`.ORG`, SREG save, `CALL main`, SREG restore/data, global initialization instructions, procedures, then `addasm`-buffered data declarations. Preserve this two-channel ordering when moving initialization.
   - Byte-for-byte output differs in whitespace for 9/10 demos; `fc /W` comparisons matching `test.bat` report NO DIFF for all 10. The batch file was not run because it deletes and recreates those `test_C` directories; equivalent comparisons used isolated outputs instead.

   6.2. ✅ Add a CodeGen API for the program intro and startup sequence. Move emission of the header, `.ORG`, optional SREG save/restore, `CALL main`, `RTN`, 
           and the `SREG` data label out of `SecondScan`. Preserve `nosave` behavior and current formatting.
   - Added `CodeGen.EmitProgramStartup(org, saveRegisters)` and replaced the startup emission block in `SecondScan`.
   - Verification: `test.bat` in an isolated copy — all 10 demos NO_DIFF; explicit `#nosave` fixture also matches the reference and omits SREG save/restore while retaining `CALL main`.

   6.3. ✅ Add a CodeGen API for global variable initialization. Pass the required variable fields as arguments and dispatch to the existing initialization routines (`varxram`, `varxarr`, `varcode`, `varfcode`, `varcarr`, `varfarr`, `varreg`, `varrarr`). 
           Preserve the existing distinction between emitted instructions and data buffered with `addasm`.
   - Added `CodeGen.EmitGlobalVariableInitialization` with explicit name/type/address/size/value and storage/initializer flags; the dispatch preserves the existing `SecondScan` conditions. Its integration into `SecondScan` is step 6.4.
   - Verification: build succeeded; `test.bat` in the isolated copy — all 10 demos NO_DIFF.

   6.4. ✅ Replace the startup and initialization emission blocks in `SecondScan` with calls to the new CodeGen APIs. 
           Keep procedure scanning and missing-`main` diagnostics in Parser.
   - `SecondScan` now delegates startup and each global variable initializer to CodeGen; procedure scanning and missing-`main` diagnostics remain in Parser.
   - Verification: build succeeded; `test.bat` in the isolated copy — all 10 demos NO_DIFF.

   6.5. ✅ Check startup output with `nosave` enabled and disabled; cover initialization declarations, arrays, and float data.
   - Normal startup and initialization output for `Array` and `Float` match the 6.1 baseline. With `#nosave`, both match the reference compiler, retain `.ORG` and `CALL main`, and omit the SREG block.
   - Verification: `test.bat` in the isolated copy — all 10 demos NO_DIFF.

7. ✅ Implement `EmitRawAsmBlock` in `CodeGen` and route `#asm...#endasm` through it.

   - In `Parser`, collect the block body through `#endasm` and pass it to CodeGen; do not emit its lines directly with `writln`.
   - Preserve the current generated-assembly contract: omit the delimiters, retain blank body lines and the existing tab/line formatting, and keep `Output`'s `outfile`/`addasm` routing unchanged. “Exact pass-through” means no changes to the assembly currently generated, not preservation of source indentation that the parser already trims.
   - Keep the blank line emitted after the enclosing statement unchanged.
   - Added `CodeGen.EmitRawAsmBlock`; `Parser.Block` now gathers each body line through `#endasm` and delegates emission without changing the existing whitespace/output routing.
   - Verification: build succeeded; `Float`, `bounce`, `bug2/main1.c`, and `bug2/main2.c` all match the reference compiler (`NO DIFF`), including blocks containing blank lines.

8. Remove remaining ASM emission from `parser.pas`, migrating each group to `CodeGen` APIs while preserving output ordering, formatting, and `outfile` routing.

   Run `test.bat` after each substep and require all 10 demos to report NO DIFF.

   8.1. ✅ Migrate the simple residual emissions: shift and increment/decrement comments in `Expression` / `Assignment`, and the trailing blank line in `Block`.
   - Use `EmitComment` and `EmitBlankLine`; preserve the exact comment text and placement.
   - `Expression` and `Assignment` now route shift/increment comments through `EmitComment`; `Block` routes its trailing blank line through `EmitBlankLine`.
   - Verification: build and `test.bat` — all 11 demos NO DIFF.

   8.2. ✅ Migrate pointer dereference, pointer store, and address-of instruction sequences in `Factor` and `Assignment`.
   - Reuse existing CodeGen pointer/address helpers where applicable; add focused APIs for any remaining instruction sequences rather than emitting ASM from Parser.
   - Verify register and XRAM pointers, byte and word pointees, and address-of expressions; add a focused fixture if the demos do not cover a case.
   - `Factor` and `Assignment` now delegate pointer loads/stores and address calculation to CodeGen. Added store-through-pointer APIs and `Demos/PointerAccess/main.c`, covering register/XRAM pointers, byte/word pointees, and address-of.
   - Verification: build and `test.bat` — all 11 demos NO DIFF.

   8.3. ✅ Migrate `DoSwitch`.
   - Preserve the current `outfile` toggling and the ordering/routing of case labels, case bodies, `RTN`, and `ENDCASE`.
   - `DoSwitch` now delegates switch comments, `CASE` entries, case-arm buffering, and closing output to CodeGen. Added `Demos/SwitchCase/main.c` and `Demos/SwitchCaseElse/main.c` to cover switches with and without an `else` arm.
   - Verification: build and `test.bat` — all 13 demos NO DIFF.

   8.4. ✅ Migrate conditional and transfer statements: `DoIf`, `DoGoto`, `DoLabel`, `DoBreak`, and `DoReturn`.
   - Use the existing branch and label APIs, plus `EmitInst`, `EmitComment`, and `EmitBlankLine`; preserve current label placement and return formatting.
   - These statements now delegate instruction, comment, blank-line, and user-label emission to CodeGen while retaining the existing branch targets and output formatting.
   - Verification: build succeeded; `test.bat` in an isolated copy — all 13 demos NO DIFF.

   8.5. ✅ Migrate loop statements: `DoLoop`, `DoWhile`, `DoFor`, and `DoDoWhile`.
   - Preserve loop labels, branch instructions, break targets, comments, and blank-line placement.
   - Loop comments, blank lines, and back-edge instructions now go through CodeGen; existing labels, branch targets, and break behavior are unchanged.
   - Verification: build succeeded; `test.bat` in an isolated copy — all 13 demos NO DIFF.

   8.6. ✅ Migrate procedure-related ASM emission in `SecondScan`.
   - Move procedure labels/comments, `RTN` instructions, skipped-procedure comments, and associated blank lines to CodeGen; keep missing-`main` and stack-corruption diagnostics in Parser.
   - `SecondScan` now delegates procedure labels, skipped-procedure comments, `RTN`, and blank lines to CodeGen; diagnostics remain in Parser.
   - Verification: build succeeded; `test.bat` in an isolated copy — all 13 demos NO DIFF.

   - Scope: diagnostic/debug `writeln`, file serialization, and commented-out legacy code are not ASM emission and are not part of this step.

---
## Task 4.2: Fixing compiler issues

This task addresses preexisting compiler bugs discovered during refactoring.

*Possibly not required if embracing the Flex/Bison rewrite (Phases 5 on)*

**Step 4.2.1 — Wrong array init code**  ✅   

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

**Step 4.2.2 — Fix word array XRAM expression parsing**

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


**Step 4.2.3 — Complete fix to "Possible Stack corruption!" warning**

**Problem description:**

The "Possible Stack corruption!" warning is caused by unbalanced PUSH/POP instructions.
Fix 4.1.1 possibily incomplete.

**Symptoms:**

The warning appears during compilation of the Array demo, for example.

It also appears in `Demos/ProcCallStack/main.c` for the float-argument call `consumeFloat(1.25)`. 
Both the reference and refactored compilers report `main: Possible Stack corruption!` and emit identical assembly, 
so this case remains a compiler issue rather than a regression from the `ProcCall` refactor.



---
# Phase 5 — Migrating to Flex/Bison

This phase replaces the syntax-directed Pascal frontend with a standard-C frontend built using Flex and Bison,
while incrementally porting the proven SC61860 backend behavior. It is divided into three subphases: define the language and backend contract,
stabilize the Pascal backend boundary, then build and validate the C compiler.

## Phase 5.1 ✅ — Define the C frontend/backend contract

Motivation: the current parser is a syntax-directed translator — it emits code during parsing.
To replace the frontend with a standard C frontend (Flex/Bison) we need an intermediate AST that completely decouples parsing from codegen.

> Change of direction compared to littleC: the goal is NOT to replicate littleC's idiosyncrasies (non-standard operator precedence, `loop`, `switch` with custom semantics, etc.) but to converge toward a subset of the C89/C99 standard, within the limits of the SC61860 platform. Existing littleC programs may require minimal adjustments to compile with the new frontend.

**Assessment — using the existing `CodeGen.pas`**

`CodeGen.pas` is no longer just a collection of low-level helpers. It already contains a substantial, tested SC61860 backend: instruction and label emission, byte/word/float loads and stores, local stack addressing, arrays, pointers, procedure-call stack handling, comparisons, branches, arithmetic/library calls, and global data initialization. Phases 5.1–5.3 should treat this as the implementation reference and compatibility contract, rather than plan a fresh backend from scratch.

It is not directly reusable by a C/Flex/Bison program as-is: the unit is Pascal, emits through `Output.WritLn`, and depends on shared mutable state such as `pushcnt`, `isword`, `isfloat`, `optype`, labels, and library-use tables. The practical reuse strategy is **informed, incremental implementation**: reuse proven instruction sequences and target conventions where suitable, give the C backend explicit state/output interfaces, and implement only the operations required by the documented C subset and current milestone. Do not mechanically translate the entire unit or introduce Pascal/C FFI as a prerequisite.

| Existing capability in `CodeGen.pas` | How Phase 5 should use it |
|---|---|
| Emission, labels, branches, comparisons, arithmetic and library selection | Use proven SC61860 sequences as implementation guidance; define C API names and expected behavior from the C subset. |
| Byte/word/float variables, arrays, pointers, locals and stack/call sequences | Consult as a reference for applicable target conventions. Keep type/address classification in the compiler and pass explicit parameters/state to backend routines. |
| Global data initialization and program startup/finalization | Use established assembler directives and validate output order and runtime behavior against the C backend contract. |
| `Output` buffers and `outfile` routing | Replace implicit global routing with an explicit C output context/buffer; test switch buffering and library text separately. |
| Existing optimizer transformations in `SecondScan` | Keep distinct from instruction-generation helpers; extract and test them in Phase 5.2 before porting the optimizer to C. |

The language direction remains a standard-C subset for SC61860 rather than a reimplementation of littleC semantics. Existing programs may need minimal syntax changes; validate C behavior against the documented subset and functional tests.


### Task 5.1.1 ✅: Define the target C subset

Created `Docs/sc61860_c_subset.md` as the language specification, including supported and deferred C features, target data model,
semantics, preprocessor behavior, SC61860 extensions, and deprecated littleC syntax. Keep detailed language requirements in that document;
use this roadmap for implementation sequencing and acceptance criteria.

### Task 5.1.2 ✅: Define a typed AST (standard C)

Added `Source/C/lcc2/ast.h` with typed nodes for the target declarations, types, statements, initializers, and expressions.
Nodes retain source spans, and recursive declarators use structured type kinds instead of string-valued types.

### Task 5.1.3 ✅: Document formal BNF grammar

Added `Docs/sc61860_c_grammar.y`, a Bison-compatible C89-subset grammar with standard C expression precedence, SC61860 extensions,
and no function-pointer or multidimensional-array declarators. Bison generates it without shift/reduce or reduce/reduce conflicts.

### Task 5.1.4 ✅: Define the backend compatibility contract

The contract and operation inventory are documented in `Docs/sc61860_backend_contract.md`, including explicit C context invariants, 
library behavior, and named Pascal fixtures. The C backend's first milestone is intentionally smaller than the full Pascal API; 
signed operations and standard-C switch semantics require new implementations.

---
## Phase 5.2 — Stabilize and test the backend boundary

Phase 5.2 is a bridge, not a second backend rewrite. Extract the existing
optimizer from `SecondScan` into a separately testable Pascal unit. Test the C
optimizer independently against its own input/output contract; Pascal optimizer
cases are not C migration acceptance tests.

### Task 5.2.1 ✅: Extract optimizations from `SecondScan` into `Backend.pas`

Extracted the existing optimizer into `Backend.pas`. `SecondScan` now calls `OptimizeAssembly`, which applies the passes in their existing order in memory and writes the final assembly directly to the requested output file.

| Step | Pattern → Replacement |
|------|-----------------------|
| 1 | `EXAB; EXAB` → remove |
| 2 | `LIA x; EXAB` → `LIB x` |
| 3 | `PUSH; LIB x; POP` → `LIB x` |
| 4 | `LIB 1; LP 3; ADM; EXAB` → `INCA` |
| 5 | `LIB 1; LP 3; EXAB; SBM; EXAB` → `DECA` |

Exposed testable functions: `OptimizeDoubleEXAB()`, `OptimizeLIAtoLIB()`, `OptimizeRedundantPUSHPOP()`, and `OptimizeIncDec()`. 
The existing constant-arithmetic folds are also retained in the pipeline. Temporary `temp.asm`/`temp2.asm` optimizer files are no longer needed.

### Task 5.2.2 ✅: Unit tests for each peephole optimization pattern

Added `backend_tests.pas` and `test_backend.bat`. 
The optimizer tests cover matching and near-match cases for each pass and verify that the full pipeline preserves unrelated instructions, 
labels, comments, and library text.

---
## Phase 5.3 — Incremental C compiler using the existing backend as reference

Prerequisites: target language and grammar defined (Tasks 5.1.1–5.1.3), backend compatibility contract and fixtures established (Task 5.1.4), and Pascal optimizer extracted/tested (Phase 5.2).

> From this phase the work is on a new C codebase (`Source/C/lcc2/`), parallel to the Pascal one. The Pascal compiler remains a separate codebase; its outputs are not a C migration acceptance oracle.

The C backend implements relevant SC61860 operations from `CodeGen.pas` as needed by the documented C subset. Its behavior and tests are defined by the C contract, not by reproducing Pascal output. A direct Pascal/C FFI is out of scope unless a later measured need justifies its build and runtime complexity.

### Task 5.3.1 ✅: Project setup in C

Created the `Source/C/lcc2/` project with a GCC/Make build, command-line entry
point, AST arena allocation, and initial allocation/CLI smoke tests. The
parser, symbol table, backend, optimizer, output, and platform modules are
introduced by their subsequent milestones.

- Verification: `make` builds `lcc2.exe`; `make test` passes AST arena and CLI checks.

### Task 5.3.2 ✅: Flex lexer (`lexer.l`)

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

Keywords that are not supported (`long`, `volatile`, `register`, etc.) are
recognized by the lexer but generate a semantic error in the parser
("unsupported on this platform").

Implemented `lexer.l` with token codes in `lexer_tokens.h`, source locations,
comments, literals, operators, SC61860 attributes, inline-assembly blocks, and
the supported `#pragma` forms. Added scanner coverage to the project test
target; typedef-name classification remains tied to the parser's scope state.

- Verification: `make test` passes the scanner, AST arena, and CLI checks.

### Task 5.3.3 ✅: Bison parser (`parser.y`)

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

Implemented `Source/C/lcc2/parser.y` with conflict-free expression precedence,
AST actions for declarations, aggregate types, initializers, statements,
function definitions, casts, calls, and literals, plus SC61860 storage
attributes and explicit diagnostics for unsupported storage/type keywords.
Added the `lcc2_parse_string` API, parser build rules, and AST-shape tests.
Full symbol-table shadowing and semantic validation remain follow-up work.

- Verification: `make test` passes lexer, AST, parser, and CLI checks; Bison
  runs with `-Wall -Werror` and reports no grammar conflicts.

### Task 5.3.4: SC61860 backend — tree-walker (`codegen.c`)

Use `CodeGen.pas` as a source of proven target instruction sequences and legacy behavior, porting operations incrementally as the C frontend needs them. Do not translate every routine or reproduce every littleC feature. Maintain an operation-coverage checklist tied to the supported C subset and the backend slices implemented here; Task 5.1.4 fixtures are useful examples, not a required feature list.

The C API should retain the semantic groupings of the Pascal API, but pass backend context explicitly instead of relying on unit globals:

```c
typedef struct CodegenContext CodegenContext;

// Emission and labels
void emit_inst(CodegenContext *ctx, const char *inst);
void emit_inst_op(CodegenContext *ctx, const char *inst, const char *operand);
void emit_inst_op_comment(CodegenContext *ctx, const char *inst,
                          const char *operand, const char *comment);
void emit_comment(CodegenContext *ctx, const char *comment);
void emit_label(CodegenContext *ctx, const char *label);
char *new_label(CodegenContext *ctx);

// Port scalar/array/pointer access, calls/stack, branches,
// arithmetic and data operations in the milestones that need them.
void codegen_translation_unit(CodegenContext *ctx, TranslationUnit *tu);
```

`CodegenContext` owns output buffer(s), label counter, stack-depth accounting, library-use state, and per-function code-generation state. Prefer explicit arguments for type, storage class, address, and symbol name; do not mirror Parser globals in the new backend.

For each implemented operation:

- Preserve the existing instruction sequence and formatting first, unless standard-C semantics require a change.
- Verify generated assembly and behavior against the documented C contract.
- Test stack-depth and library requirements as well as output text.
- Keep AST-level concerns (type checking, symbol lookup, layout) outside the instruction emitter; pass their results to backend operations as storage/address inputs.

#### Task 5.3.4.1 ✅: Initial emission and constant-expression slice

Implemented in `Source/C/lcc2/codegen.c`: explicit output, label, and
stack-depth context; byte/word constant loads and stack primitives; byte/word
add/sub operation sequences; and a validating tree-walker for parameterless
functions returning constants or `+`/`-` expressions. Unsupported
translation-unit constructs are rejected before assembly emission.
`tests/test_codegen.c` checks expected instruction text, stack accounting,
labels, and rejection of unsupported constructs. This slice does not provide
complete-program output or command-line source compilation.

#### Task 5.3.4.2 ✅: Executable ASM and PC-1403 smoke test

Bring forward the minimal startup/output integration needed to run the
Task 5.3.4.1 subset on the target, without waiting for scalar storage,
control flow, or general stack frames.

- Wire source-to-ASM compilation into the CLI using the existing parser and
  backend, with explicit diagnostics for unsupported constructs.
- Emit complete PASM-compatible assembly with a configurable origin,
  register save/restore following the Pascal reference, `CALL main`, and
  an explicit `RTN` back to BASIC.
- Add a minimal fixture such as
  `unsigned int main(void) { return 40 + 3 - 1; }`, without variables,
  runtime libraries, or ROM calls, to exercise arithmetic and the evaluation
  stack as well as program entry/return.
- Acceptance: PASM assembles the output; loading it into reserved PC-1403
  memory and executing `CALL &E030` returns to a responsive BASIC prompt,
  including after repeated calls. In MAME, also verify the result is `42`
  before register restoration and that the stack is balanced; absence of a
  crash alone does not establish arithmetic correctness.

Binary generation remains the assembler's responsibility. Global
initialization and full code/data/library integration remain in Task
5.3.4.7; this early slice does not complete that milestone.

Implemented the CLI and minimal startup wrapper, with backend/CLI regression
coverage and `tests/smoke.c` / `tests/smoke.lua`. The generated
78-byte binary assembles with PASM. Memory/register-only MAME verification
passes ten consecutive machine-code calls: BA is `42` before restoration,
R is `94` on return from main and `96` on return to the caller, and all
twelve register bytes in the fixture are restored (the fixture initializes I
to 11, so this does not verify preservation of an arbitrary incoming I).
This also exposed and fixed a
missing `LP 0` before word `ADB` in the Task 5.3.4.1 C emitter.

The user confirmed the BASIC-level smoke procedure passed in their PC-1403
MAME session after building and deploying the binary: repeated execution,
BASIC program retention, and interpreter responsiveness. The initial
apparent memory corruption was reported before the binary had been loaded,
so it is not evidence of a generated-code defect. No physical-device
execution has been verified.

#### Task 5.3.4.3 ✅: Minimal scalar symbols and storage layout

Bring forward the scalar prerequisite from Task 5.3.5 before extending the
tree-walker. Implement it in `symtab.c`, separately from instruction emission.

- Resolve byte/word scalar declarations and identifier uses with C
  case-sensitive lookup, block scopes, and local-over-global shadowing.
- Produce explicit type, width, storage-class, fixed-address, and local-offset
  results for backend consumers.
- Validate duplicate declarations, overlapping/out-of-range fixed placements,
  undeclared identifiers, and assignment to non-modifiable objects.
- Establish and document the local layout contract for parameterless leaf
  functions. Keep frame allocation/cleanup in the subsequent backend slice,
  not in symbol resolution.
- Cover the resolver independently, including scope exit and shadowing.
  Unsupported declaration and expression forms must fail explicitly.

This is a partial extraction of Task 5.3.5, not completion of the full symbol
table. General allocation, parameters, aggregate layout, and tag namespaces
remain follow-up work.

Implemented `symtab.h` / `symtab.c` with independent resolver coverage in
`tests/test_symtab.c`. The initial subset requires explicit fixed addresses
for global scalars, separates register and XRAM overlap checks, and lays out
block-scoped locals in a single function-wide frame (initially capped at
32 bytes). Local offsets follow the Pascal reverse-word stack convention;
the backend owns allocation and cleanup. Combined `__xram __at(...)`
declarations and unsigned plain `char` are wired through the parser.

#### Task 5.3.4.4 ✅: Scalar storage access

Port byte/word loads and stores for register, local-stack, and XRAM storage.
Consume explicit symbol/type/layout results from Task 5.3.4.3 rather than
performing allocation or symbol lookup in the instruction emitter.
Cover assignments and scalar expressions in the tree-walker.
Include the minimal frame allocation/restoration needed to exercise local
scalars in parameterless leaf functions; general calls and frames remain in
Task 5.3.4.6. Global initialization and symbolic XRAM data allocation remain
in Task 5.3.4.7. Validate storage primitives, byte promotions/conversions,
assignment-expression values, and stack-relative access while expression
temporaries are live.

Implemented explicit-location byte/word load/store primitives and the
scalar tree-walker, including leaf frame allocation/cleanup, byte promotion,
local initialization, nested-block shadowing, and assignment values.
Instruction tests cover the LP/LIP boundary, XRAM page crossings, local
offsets with live temporaries, and invalid locations. CLI checks cover the
scalar source path and preservation of output on rejected input.
`tests/scalars.c` assembles to 436 bytes. Using `pc1403_key.lua`,
`tests/scalars.lua` reserves memory, enters a BASIC caller, deploys the
binary, and verifies ten RUNs through memory dumps: `2A 00 3B FF` at `&E600`
and `5E 12` across `&E7FF`/`&E800`, with BASIC program bytes unchanged.
Global initialization, automatic/symbolic global allocation, parameters,
and general calls remain explicitly unsupported.

#### Task 5.3.4.5 ✅: Comparisons, branches, and structured control flow

Port equality and unsigned relational operations and branch helpers.
Extend the tree-walker with conditionals and loops, including correct
`break`, `continue`, and standard-C `for` step ordering.

Implemented equality and unsigned relational expression emission through the
existing 16-bit comparison routines, with zero/nonzero branch handling across
both accumulator bytes. The resolver and tree-walker now support `if`/`else`,
`while`, `do`/`while`, expression-initialized `for`, `break`, and `continue`;
`continue` in a `for` loop targets its step expression, which executes after
the body. Signed relational comparisons are rejected. Tests cover all
comparison helpers, nested control transfers, out-of-loop `break`/`continue`,
and `for` step ordering. Runtime-library output integration remains in
Task 5.3.4.7.

#### Task 5.3.4.6 ✅: Functions, calls, and stack frames

Add parameters, local storage, calls, return-value handling, and stack
allocation/restoration. Verify cleanup thresholds, nested calls, and
byte-accurate stack balance at function boundaries.

Implemented named byte/word parameters by value, matching scalar prototypes,
direct calls to translation-unit definitions (including recursion and nested
calls), byte/word return conversion, and local-frame allocation/restoration.
Parameter offsets account for total argument bytes and the callee frame;
caller cleanup preserves return values and uses individual pops for void-call
argument sizes below four bytes, otherwise adjusting the stack pointer.
Resolver/codegen tests cover mismatched prototypes, argument counts,
parameter layout, nested calls, byte/word/void results, both cleanup paths,
and zero stack depth at function boundaries. Local frames remain capped at
32 bytes, and complete-program `main` must remain parameterless. External
runtime linkage and combined code/data/library output remain in
Task 5.3.4.7.

#### Task 5.3.4.7 ✅: Program startup, data, and output integration (umbrella)

This is the parent milestone, not a separate implementation slice. It is
complete only when all supported program parts are integrated into valid
PASM assembly: startup, initialized data, generated functions, and any
required runtime libraries in a deliberate output order. Binary generation
remains the assembler's responsibility.

Subtask 5.3.4.7.1 delivers only fixed-address scalar global initialization.
It emits the initialization stores in startup before `CALL main`; it does not
separate code/data/library output channels, include runtime libraries, or
complete this parent milestone. The remaining work is divided into subtasks
5.3.4.7.2 and 5.3.4.7.3 below.

##### Task 5.3.4.7.1 ✅: Fixed-address scalar global initialization

Implement C initialization semantics for the fixed-address byte/word scalar
globals currently accepted by the resolver. Keep automatic allocation,
arrays/aggregates, floating-point objects, and symbolic data allocation out
of this subtask.

- Resolve explicit global initializers only when they are scalar constant
  expressions supported by the current integer/character and `+`/`-` subset.
  Reject list, floating-point, variable-dependent, and otherwise unsupported
  initializers with a diagnostic before emitting output.
- Apply target-width conversion consistently: byte objects store the low
  eight bits and word objects store the low sixteen bits, including signed
  declarations. Preserve the existing fixed register/XRAM address and
  overlap validation.
- Give globals without an explicit initializer C's zero-initialization
  behavior. Emit the startup stores for both register and XRAM objects before
  `CALL main`, without changing the existing startup save/restore contract.
- Add resolver and code-generation tests for constant values, byte/word
  truncation, zero initialization, both storage classes, startup ordering,
  and rejected initializer forms. Compile representative initialized-global
  fixtures and require PASM to assemble the resulting output.

Do not mark this subtask complete until the tests establish that all global
initialization occurs before `main` and that rejected inputs leave existing
CLI output untouched. PC-1403 runtime verification should confirm initialized
values if the fixture can be exercised without disturbing the active MAME
session.

Implemented the resolver's scalar constant-initializer checks and startup
stores for all fixed-address scalar globals. Integer/character constants and
`+`/`-` expressions are reduced modulo 16 bits, then truncated to the declared
byte/word width; uninitialized globals are stored as zero. Initialization
runs after the optional register save and before `CALL main` for both register
and XRAM locations. Resolver/codegen tests cover character and integer
constants, byte truncation, signed word `0 - 1`, zero initialization, storage
classes, and ordering. CLI rejection checks preserve existing output, and
`tests/global_init.c` assembles with PASM. PC-1403 runtime execution was
not part of this validation.

##### Task 5.3.4.7.2 ✅: Runtime-library dependency tracking and inclusion

Make complete assembly self-contained for runtime routines called by the
currently supported compiler subset, especially comparison helpers.

- Record each runtime routine required during code generation in the
  compilation context, deduplicating repeated uses.
- Emit or append the corresponding PASM-compatible routine definitions once,
  after generated code, with labels and calling conventions matching the
  legacy runtime library. Fail explicitly if a required routine cannot be
  resolved or included; do not silently leave external calls in output.
- Keep runtime-library code distinct from generated function output so the
  final program assembly can place and inspect it deliberately.
- Test individual and repeated helper use, deduplication, no-library programs,
  and unresolved helper failures. Assemble comparison-containing programs
  with PASM and validate comparison behavior in the PC-1403 test harness.

This subtask supplies helper definitions; it does not restructure all output
channels or declare Task 5.3.4.7 complete.

Implemented `lcc2` runtime tracking in `CodegenContext`, deduplicating the six
comparison dependencies. A built-in catalog preserves the legacy `.lib`
labels and unsigned B:A/J:I convention without requiring external library
files at compilation or assembly time. Runtime text stays separately
inspectable through `codegen_runtime_output`; complete-program generation
appends it once after generated functions. Unknown routine requests and
function collisions with included runtime labels fail explicitly.

Host and CLI tests cover individual/repeated dependencies, no-library
programs, unresolved routine errors, runtime-label collisions, and preservation
of existing CLI output on failure. `make test` passes, and PASM assembles the
self-contained `tests/comparisons.c` fixture. On 2026-10-23,
`tests/comparisons.lua` passed 130 PC-1403 MAME calls: all six operators
over 13 operand pairs repeated ten times, covering equality, zero/65535,
low-byte ordering, high-byte ordering, and the 32767/32768 unsigned boundary.
Every return checked stack balance and wrapper register restoration.

Earlier changes to the Pascal reference also embed its requested `.lib`
sources. Those changes alone did not implement this C-backend task. General
output-section ordering and a responsive BASIC-caller smoke test are covered
separately by Task 5.3.4.7.3.

##### Task 5.3.4.7.3 ✅: Ordered output sections and end-to-end program integration

Integrate startup, global initialization, generated functions, data
declarations, and the runtime routines from Task 5.3.4.7.2 into one
deterministically ordered PASM source file.

- Keep startup, function code, data, and library text in distinct buffers or
  equivalent explicit sections until final assembly-text composition.
- Define and document the order and placement rules, including startup data
  such as `SREG`, global initialization before `CALL main`, and library code
  after generated functions. Ensure labels and references remain unique.
- Wire the complete output through the CLI and add fixtures that combine
  initialized globals, calls, comparisons, and control flow. Require PASM to
  assemble these fixtures and retain the existing CLI guarantee that failed
  compilation does not replace output.
- Complete the PC-1403 runtime smoke check for startup, initialized globals,
  helper calls, stack balance, and return to a responsive BASIC caller.

The parent milestone is complete only after 5.3.4.7.1, 5.3.4.7.2, and
5.3.4.7.3 pass their stated acceptance checks.

Implemented separate startup, function, data, and runtime buffers, exposed
through `codegen_section_output` and concatenated in that order. Global
initialization precedes `CALL main`; functions retain source order; the
optional `SREG` area follows functions; required helpers follow data.
Host tests verify exact section composition, deterministic output, `nosave`,
and context-reuse rejection. CLI tests assemble the combined fixture and
confirm that failed compilation preserves existing output. `make test`
passes, and PASM assembles `tests/program.c` to 486 bytes.

In a fresh disposable PC-1403 MAME session, `tests/program.lua` passed
ten BASIC `RUN`s and ten machine-code returns. Each run verified initialized
and zeroed globals, nested function frames, a loop, comparison helper calls,
balanced entry/return stack pointer (R=86), and wrapper register restoration.
Outputs at `&E600` were `37 12 23 00 FF FF FF 2A`: the final byte came from a
keyboard-entered BASIC `POKE` after return. BASIC program bytes and its start
pointer remained unchanged. The harness stages its final return trap only
after generated functions finish, avoiding corruption of their entry bytes.
All three subtasks have now passed their acceptance checks, completing the
parent milestone.

#### Task 5.3.4.8 ✅: Array access

Port single-dimensional array indexing and element loads/stores by storage
class and element width. Extend the resolver to provide array extents,
element types, and storage locations; the emitter consumes those results
rather than performing lookup or layout. Cover index scaling, reads, writes,
and the supported initialization forms. Keep multidimensional and
variable-length arrays explicitly unsupported unless separately planned.

Implemented fixed register/XRAM globals and function-local
byte/word arrays: positive constant extents (or extent inference from a
nonempty brace list), overlap/frame-size accounting, constant and runtime-index
reads/writes, and brace-list initialization with zero fill for omitted elements.
Runtime subscripts accept supported integer expressions and are evaluated
once; address calculation retains full 16-bit indices and word-element scaling.
Constant out-of-bounds subscripts are rejected. Runtime bounds are unchecked,
as in C; multidimensional/variable-length arrays and array-to-pointer decay
remain explicitly unsupported.

`make test` passes resolver, codegen, CLI, and PASM checks, including rejection
without replacing existing CLI output. `tests/arrays.c` and
`tests/arrays_wide.c` assemble to 1964 and 3721 bytes respectively.
Using `tests/arrays.lua`, both fixtures passed ten BASIC RUNs and ten
machine-code returns in PC-1403 MAME. The first covers byte/word register,
XRAM, and local reads/writes, zero initialization, page crossings, signed-byte
promotion, assignment values, and single evaluation of an index function.
The wide-index fixture, rerun after a device reset, verifies byte index 256,
word index 128 (byte offset 256), and expression indices; outputs were
`FF 00 EF BE FF 00 00 00`. Both verify stack balance (R=86), register
restoration, BASIC program retention, and a responsive BASIC caller.

#### Task 5.3.4.9 ✅: Compact array initialization (code-size follow-up)

Prioritize this follow-up before the backend completion gate in Task
5.3.4.14; do not defer it to Phase 6. Task 5.3.4.8 remains functionally
complete, but its per-element initialization stores are too costly for
larger arrays on the PC-1403. The 3721-byte for `arrays_wide.c` declares
526 bytes of globals and currently spends roughly 3.1 KiB in startup,
mostly zero initialization.

- Replace repeated zero stores for sufficiently large contiguous regions
  with compact fill loops or a shared routine. Keep short initializations
  inline when that produces smaller output; determine the crossover from
  assembled byte counts rather than assuming a loop is always cheaper.
- Preserve C zero initialization, explicit initializer values, and zero-filled
  tails of partial brace lists. Cover register/XRAM globals and initialized
  local arrays where applicable, without zeroing uninitialized local arrays.
- Preserve startup-before-main ordering, page-crossing behavior, stack
  balance, and the register save/restore contract. Keep storage/layout
  decisions outside the instruction emitter.
- Record before/after PASM sizes for both array fixtures and require a
  substantial reduction for `arrays_wide.c`, with size-regression
  checks and repeated PC-1403 runtime verification of unchanged results.

Implemented shared internal-RAM and XRAM fill routines for contiguous zero
runs of at least four bytes; shorter runs remain inline. Global arrays compact
all zero runs, including explicit zero values and partial-list tails, while
initialized local arrays compact constant-zero runs and tails without touching
uninitialized local arrays. XRAM fill starts with `DY` so `IYS` correctly
handles page crossings. Nonzero initializers retain their original order.

PASM sizes are 1964 to 1896 bytes for `arrays.c` and 3721 to 646 bytes
for `arrays_wide.c` (about 83% smaller for the wide fixture). The small
fixture now also covers register, XRAM, and local zero initialization. CLI
regression limits are 1964 and 700 bytes respectively. `make test` passes.
In MAME, both fixtures passed ten BASIC calls and ten returns; the small
fixture verified zeroed register/local/XRAM arrays across an XRAM page
boundary, and the wide fixture retained outputs `FF 00 EF BE FF 00 00 00`.
Both runs verified stack balance, register restoration, and BASIC return.

This is an initialization-emission improvement, separate from the legacy
peephole port in Task 5.3.6 and the AST optimizations in Phase 6; those passes
alone should not be expected to eliminate the repeated startup stores.

#### Task 5.3.4.10 ✅: Pointer operations (umbrella)

This task is split because pointer support crosses three distinct contracts:
resolver-produced pointee metadata and address formation, pointer values in
storage and the call ABI, and indirect byte/word accesses. Keep type checking,
address resolution, and storage classification outside the instruction
emitter. The umbrella is complete only when all three subtasks and their
combined end-to-end checks pass.

##### Task 5.3.4.10.1 ✅: Pointer type resolution and address formation

- Extend the resolver to describe supported pointer types, including explicit
  pointee type, width, and address/storage information for register and XRAM
  objects.
- Form addresses for supported lvalues and reject invalid address-of operands
  and unsupported pointee types before code generation.
- Keep function pointers, pointer-to-pointer types, and other unsupported
  pointer forms explicitly rejected.
- Test resolver results and address formation independently for byte and word
  objects in register and XRAM storage.

Implemented resolver metadata for address-of byte/word scalar identifiers and
constant-index elements of global arrays in register and XRAM storage,
including the pointee type, object location, and 16-bit pointer width. The
backend emits those resolved addresses as word constants. Local objects,
whole-array addresses, dynamic array-element addresses, function pointers, and
unsupported lvalues are rejected. This address-formation slice was initially
tested as a discarded expression; pointer storage and call support are
implemented separately in 5.3.4.10.2.
Resolver and codegen tests cover both storage classes and pointee widths, plus
invalid lvalues, local addresses, pointer arithmetic, and unsupported pointer
returns.

##### Task 5.3.4.10.2 ✅: Pointer values, assignment, and parameters

- Support pointer variables and pointer assignment as address values, using
  the target's pointer representation and explicit resolver results.
- Pass pointer values as function parameters; preserve parameter layout,
  caller cleanup, nested-call behavior, and stack balance.
- Test pointer assignment/copying and pointer parameters independently of
  dereferenced access, including diagnostics for incompatible assignments.

Implemented 16-bit byte/word pointer variables in automatic locals and
explicitly placed register/XRAM globals. Local pointer initialization,
assignment, copying, and null-pointer constants use the regular scalar
load/store and call paths; pointer parameters are passed by value and use
two-byte frame offsets and caller cleanup. The resolver enforces matching
pointee base type, signedness, and qualifiers, and rejects pointer-to-pointer
types, pointer returns, pointer arithmetic, and scalar/pointer mismatches.
Global pointer initializers are limited to null constants; address relocations
and pointers to automatic locals remain unsupported.

Resolver/codegen tests cover local and global pointer storage, register and
XRAM pointer objects (including a word crossing an XRAM page boundary),
initialization/copy/assignment, compatible and incompatible function
arguments, null pointers, and zero stack depth. Pointer dereference remains
the separate scope of Task 5.3.4.10.3.

##### Task 5.3.4.10.3 ✅: Indirect pointee loads and stores

- Implement dereference reads and writes for byte and word pointees in register
  and XRAM storage. Preserve assignment-expression results and byte
  promotion/conversion behavior.
- Cover direct pointers and pointers received as parameters, including XRAM
  page-boundary accesses, register restoration, and stack balance.
- Reject unsupported pointer arithmetic explicitly rather than emitting
  partial code. Validate assembled output and PC-1403 runtime results for the
  supported combinations.

Implemented resolver metadata and indirect byte/word loads and stores for
register and XRAM pointers, including pointer parameters, assignment-expression
results, and byte promotion. Dispatch uses a zero high byte for register RAM
and nonzero high byte for XRAM; consequently, taking the address of an XRAM
object below address 256 is rejected as ambiguous. Resolver/codegen tests cover
both storage classes, parameter access, and a word crossing an XRAM page
boundary. The 842-byte `tests/pointers.c` fixture assembles through PASM
and passed ten consecutive BASIC RUNs and machine-code returns in PC-1403 MAME using
`tests/pointers.lua`. Outputs were
`0007 0007 BEEF 00A5 1234 FF80 D156 1234`, verifying indirect register/XRAM
access, assignment conversion/results, signed-byte promotion, and the XRAM
word crossing address `0xE5FF`. Each BASIC call verified stack balance (R=86
at entry and before returning to BASIC) and restoration of the wrapper's
twelve registers. After each RUN, the keyboard-entered BASIC command
`POKE&E710,6*7` produced 42; the BASIC program and memory pointer remained
unchanged. The harness reported calls=10, returns=10, and basic_checks=10.
Runtime testing exposed and fixed X/Y selection for indirect XRAM stores
and EXAM-clobbered assignment results for internal-RAM stores. The BASIC
rerun required a device Reset key after the earlier CPU-state-injecting test;
a machine soft reset alone did not restore a clean BASIC environment.

#### Task 5.3.4.11 ✅: Aggregate layout and member access (umbrella)

Split this task because compiler-owned aggregate layout, direct member access,
and pointer-based member access have separate resolver/backend contracts.
Keep type checking and layout outside the instruction emitter. The umbrella is
complete only when all three subtasks and their combined tests pass. The
initial supported subset is scalar fields of complete structures; explicitly
reject unions, aggregate copies/initializers, arrays of aggregates, incomplete
or recursive-by-value structures, and any other unsupported aggregate form.

##### Task 5.3.4.11.1 ✅: Structure layout and member metadata

- Extend the symbol table/layout work in Task 5.3.5 to compute member offsets,
  widths, and total structure size with checked arithmetic.
- Resolve supported complete structure declarations and reject duplicate or
  unsupported member layouts before code generation.
- Test computed layouts, storage bounds/overlap, and clear rejection of
  unsupported aggregate declarations independently of the backend.

Implemented resolver-owned structure metadata for tightly packed byte and
word fields in declaration order, with checked total-size/address bounds,
duplicate-name rejection, and queries for structure size and named member
type/offset/width. Global fixed-address and automatic local structure objects
reserve their full storage, while aggregate value operations, initializers,
unions, incomplete/nested/array/pointer/float member layouts, and unsupported
aggregate forms are rejected explicitly. Symbol-table tests verify mixed
signed/unsigned field metadata, comma-declared members, register/XRAM/local
storage sizing, overlap and bounds failures, and invalid layouts.

##### Task 5.3.4.11.2 ✅: Direct structure member access

- Resolve scalar member reads and writes through `.` using compiler-computed
  offsets for supported global and automatic structure objects.
- Preserve member type, signedness, storage class, and assignment-expression
  behavior; do not perform lookup or offset calculation in the emitter.
- Test byte/word fields and reject invalid, chained aggregate, and
  non-modifiable member expressions. Chaining through aggregate-valued fields
  is excluded from the current scalar-field subset.

Implemented resolver-bound direct `.` accesses for scalar byte/word members
of fixed-address global and automatic local structure objects. The backend
uses resolved locations for reads, writes, and assignment results, preserving
field signedness and byte promotion; const objects/fields, unknown members,
non-structure objects, chained aggregate selection, and `->` are rejected
explicitly. Uninitialized global structure objects are zero-initialized across
their full layout. Resolver and codegen tests cover global/local offsets,
loads/stores, assignment expressions, signedness, const rejection, and
unsupported access forms.

##### Task 5.3.4.11.3 ✅: Pointer structure member access

- Support `->` for pointers to supported structures, using resolved pointer
  and member metadata rather than emitter-side type/layout logic.
- Extend only the pointer forms needed for supported member access; reject
  unsupported pointer/aggregate combinations explicitly.
- Test member reads and writes through pointers, including parameter and
  storage cases supported by the resolver, with stack and target-runtime
  checks for end-to-end fixtures.

Implemented resolver-bound `->` access for complete structures through
supported pointer expressions. Resolved metadata carries the pointer
expression and member offset/type; code generation applies the offset before
dispatching to internal RAM or XRAM. Resolver and codegen tests cover
structure-pointer parameters and storage cases, while the PC-1403 fixture
verifies byte/word member reads and writes, stack balance, register
restoration, and BASIC responsiveness over ten calls. MAME runtime validation
passed with outputs `EF BE 7C 00 34 12`; unsupported pointer and aggregate
forms remain rejected.

#### Task 5.3.4.12: Remaining integer operators (unsigned, umbrella)

Standard-C `switch` is control flow and is tracked separately in Task
5.3.4.13. Signed integer semantics are deferred until after Task 5.3.8 (see
Task 5.3.9) and floating-point support likewise (Task 5.3.10). Until then,
operators operate on unsigned byte/word operands, and signed operands must
continue to be rejected explicitly. Runtime-routine registration and
deduplication are already provided by the code-generation context; extend
that catalog only where an operator requires a new routine.

Split because the unsupported operators fall into five groups with different
backend mechanisms (inline code, runtime routines, short-circuit control flow,
lvalue read-modify-write). Complete all five subtasks before closing the
umbrella. For every subtask: define target-width behavior for unsigned
operands, reject unsupported (including signed) cases explicitly rather than
emitting incomplete code, reuse and test the runtime-routine registry for any
helper routines, and cover expression results, side effects, and stack
balance. Each subtask adds resolver/codegen tests, a CLI assembly check and,
where behavior is non-trivial, a PC-1403 runtime fixture.

##### Task 5.3.4.12.1 ✅: Bitwise and unary operators

- `&`, `|`, `^`, `~`, unary `+`, unary `-` (two's complement) and logical
  `!` on unsigned byte/word operands, with byte and word width handling.
- Constant-operand forms and byte-to-word promotion; result 0/1 for `!`.
- Implemented with inline AND/OR, a `LIB_XOR16` runtime routine, constant
  folding in global initializers, resolver/codegen/CLI tests and the MAME
  fixture `pc1403_bitwise_unary` (16 results verified on PC-1403).
- Note: comparisons yield 255 for true while `!` yields 1; unification is
  deferred.

##### Task 5.3.4.12.2 ✅: Shift operators

- `<<` and `>>` (logical right shift) with constant and variable counts.
- Counts of 16 or more (or above 255) yield 0; a zero count is the identity.
- Implemented via `LIB_SHL16`/`LIB_SHR16` runtime routines (count in J:I,
  loop on RC/SL/SR); constant folding in global initializers; MAME fixture
  `pc1403_shifts` (16 results verified on PC-1403).
- Signed `>>` is rejected until Task 5.3.9 (arithmetic shift).

##### Task 5.3.4.12.3 ✅: Logical `&&`, `||` and comma

- Short-circuit evaluation with 0/1 results, usable as values and in
  `if`/`while`/`for` conditions; operands are tested one at a time so the
  evaluation stack stays balanced. Comma evaluates left then right.
- Verified on PC-1403 with fixture `pc1403_logical` (short-circuit side
  effects, high-byte-only truthiness, precedence, comma in `while`).
- Void calls remain rejected as operands (including the left of a comma).

##### Task 5.3.4.12.4 ✅: Multiplication, division and remainder

- `*`, `/`, `%` on unsigned byte/word operands, reusing the legacy
  `mul16.lib` and `div16.lib` algorithms as registry routines `LIB_MUL16` and
  `LIB_DIV16`/`LIB_MOD16` (`mul16.lib` loop branch corrected to a backward
  `JRNZM`).
- `*` truncates to 16 bits. Constant zero divisors are diagnosed; a runtime
  zero divisor yields quotient 0xFFFF and remainder equal to the dividend.
- `/` and `%` on signed operands are rejected until Task 5.3.9.
- Verified on PC-1403 with fixture `pc1403_muldiv` (16 results).

##### Task 5.3.4.12.5 ✅: Compound assignment and increment/decrement

- `+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `^=`, `|=`, and
  prefix/postfix `++`/`--` on variables, array elements, members and pointer
  dereferences.
- Implemented by rewriting to `lvalue = lvalue op rhs` (`symtab_update_rewrite`,
  shared by resolver and code generator). Lvalues with side effects (calls,
  assignments, nested updates) are rejected, so the double evaluation is
  exact. Postfix saves the old value on the stack; a discarded postfix
  (statement, `for` clause, comma left) costs nothing extra.
- Pointer variables support `++ -- += -=` with the pointee size scaling
  (structure size included). Other pointer operators are rejected.
- Signed `/= %= >>=` follow the existing signed-operator restrictions.
- Verified on PC-1403 with fixtures `pc1403_update_ops`, `pc1403_incdec`,
  `pc1403_update_arrays` and `pc1403_update_members`.

#### Task 5.3.4.13 ✅: Standard-C switch statement

Switch resolution and emission use the standard-C source-order arm layout:
dispatch enters a matching label, execution
falls through subsequent arms, and `break` exits the nearest loop or switch.
`continue` searches past intervening switch frames for the nearest loop.
Switch expressions are evaluated once; case/default labels are checked per
switch, with nested switches isolated.

The backend prioritizes compact PASM dispatch over inline comparison chains.
Unsigned byte controls use one `CASE` table on A. Unsigned word controls use
staged high-byte and low-byte `CASE` tables; the high-byte table selects a
low-byte table. Each ETC entry stores an 8-bit match value and a 16-bit target;
PTC loads the one-byte entry count into H, limiting each table to 255 entries.
Word switches may exceed 255 total labels if their high-byte table has at most
255 groups and each low-byte table has at most 255 entries. Each table carries
an explicit PASM `ELSE` entry (syntax `ELSE<TAB>label`; `ELSE:` is rejected by
PASM).

`CASE` is `PTC`+`ETC`: PTC pushes the address after `ENDCASE` onto the CPU
stack, and every target (including `ELSE`) is entered with that return address
still pushed (`ETC` jumps without popping; confirmed against MAME `scops.hxx`).
The codegen therefore treats each switch body as a
subroutine: `ctx->stack_depth += 2` inside the body, the body end and `break`
emit `RTN` (back to the post-`ENDCASE` `JP done`), `continue` and `return`
first `POP` the outstanding CASE bytes (`LoopLabels.stack_depth`/`is_switch`),
and a default reached through a staged word dispatch is `CALL`ed. Word
dispatch keeps the low byte on the stack (`PUSH` before the high-byte `CASE`,
`POP`x3 in each group stub: 2 CASE bytes + low byte), because B is not
preserved across `CASE`.

PASM fix: a forward-referenced `ELSE label` was patched one byte too early
(the value-entry `dec(codpos)` adjustment was also applied to `ELSE`, which
has no value byte), overwriting the low target byte of the preceding entry and
leaving a stray `NOP`. Word dispatch, whose group and default labels are all
forward references, therefore jumped into ROM. `Source\pasm\pasm.dpr` now
applies the adjustment to value entries only, and the root `pasm.exe` was
rebuilt with FPC 3.2.2 (`fpc -Mdelphi -Pi386 -Twin32 pasm.dpr`). All other
lcc2 fixtures assemble byte-identically.

- Verified on PC-1403 with fixture `pc1403_switch` (`switch.lua`):
  10 BASIC `CALL`s, results at `&E720` =
  `0B 00 16 00 01 00 21 00 2C 00 37 00 04 00 47 00`, call count 1 per run,
  `status="PASS"`.

Signed controlling expressions and negative signed case labels are rejected
explicitly until Task 5.3.9. The resolver also rejects nonconstant case labels,
duplicate cases/defaults, out-of-range byte cases, and case/default outside a
switch. Targeted code-generation tests cover fallthrough, evaluation-once,
nested switch labels, word staging, and continue routing; the CLI fixture is
assembled by PASM, and the MAME runtime harness reports `status="PASS"`.

#### Task 5.3.4.14 ✅: Backend coverage checklist and completion gates

The acceptance baseline is the supported C subset and the backend operations
implemented in Tasks 5.3.4.1 through 5.3.4.13, not the full legacy feature set.
The Task 5.1.4 fixtures below are historical examples for overlapping behavior;
they do not require `lcc2` to support every legacy construct. Validate the
documented C semantics through resolver, codegen, CLI/PASM, or target-functional
tests as appropriate.

| Backend area and related reference fixtures | `lcc2` implementation and evidence | C-contract status |
|---|---|---|
| Byte/word scalar access, constants, arithmetic: `test_C/LoadVariable/main.c`, `test_C/Math/main.c`, `test_C/16bitdiv/main.c` | Unsigned byte/word scalar and arithmetic subset; resolver/codegen checks and `Source/C/lcc2/tests/scalars.c` | Supported subset is tested and target-validated. Signed semantics remain deferred to 5.3.9. |
| Register/XRAM arrays, byte/word elements, initialization: `test_C/Array/main.c` | Fixed-address and local arrays, initialization and full-width indexing; `test_codegen.c`, `arrays.c`, `arrays_wide.c` | Supported forms are tested and target-validated; local arrays, 16-bit index scaling, and runtime-index support follow the C implementation contract, regardless of legacy behavior. |
| Pointer address, dereference, indirect stores: `test_C/pointer/main.c`, `test_C/PointerAccess/main.c` | Supported global/array addresses, pointer values and parameters, and indirect byte/word accesses; `pointers.c` and `.lua` | Supported subset is tested, including XRAM page crossing and stack restoration. Unsupported pointer forms are rejected explicitly. |
| Branches, loops, switch: `test_C/Loop/main.c`, `test_C/bounce/main.c`, `test_C/SwitchCase/main.c`, `test_C/SwitchCaseElse/main.c` | Comparisons and structured control flow in 5.3.4.5; standard-C switch in 5.3.4.13; `switch.c` / `.lua` | Codegen tests cover `for` step ordering and nested `continue` routing; the unsigned switch is target-validated. Standard-C fallthrough and loop behavior are intentional C semantics. Signed switch and negative signed cases are deferred to 5.3.9. |
| Float constants and arithmetic: `test_C/Float/main.c` | Not supported by the current backend slice | Explicitly deferred to 5.3.10; rejection is required until implementation and ABI/runtime tests are complete. |
| Calls, arguments, local storage, return values, stack thresholds: `test_C/Parameter_passing/main.c`, `test_C/ProcCallStack/main.c` | Byte/word parameters, calls, frames, and return values; codegen tests and `program.c` / `.lua` | Supported subset is functionally validated for stack balance and caller restoration. Floating-point calling convention is deferred to 5.3.10. |
| Structure layout and member access (C feature beyond the Task 5.1.4 inventory) | Compiler-computed offsets for supported scalar fields and `.` / `->`; resolver/codegen tests and `struct_members.c` / `.lua` | Supported subset is implemented and functionally validated; unsupported aggregate forms are explicit errors. |

Cross-cutting gates apply to every row: verify emitted runtime-library
dependencies are complete and deduplicated, and check evaluation-stack
accounting at expression and function boundaries. Resolver/codegen tests cover
instruction shape and stack effects; CLI/PASM and target tests validate
complete output and runtime behavior. The evidence and explicit unsupported
forms are recorded in the corresponding subtasks above.

This checklist is complete: every backend slice currently in scope has
implementation and test evidence, while features outside that scope are
identified as deferred or explicitly rejected. This closes the 5.3.4 backend
coverage gate; it does not claim complete conformance to the broader language
specification in `Docs/sc61860_c_subset.md`. Signed semantics (5.3.9) and
floating point (5.3.10) remain later implementation work required by the final
port-completion gate in 5.3.11.

### Task 5.3.5 ✅: Symbol table (`symtab.c`)

Complete for the documented `lcc2` language subset. This task does not require
all C89 name or storage semantics: the goal is a small, reliable compiler for
the SC61860, and features outside the agreed subset remain deferred unless a
target use case or a later roadmap task requires them.

- Resolve case-sensitive identifiers and functions, nested block scopes,
  local shadowing, parameters, and function declarations/definitions.
- Provide backend-consumable storage and layout information for supported
  globals, parameters, local frames, arrays, restricted structures and member
  access, while keeping resolution separate from instruction emission.
- Validate supported-subset constraints and reject unsupported or invalid
  declarations and uses. `tests/test_symtab.c` covers resolver behavior;
  `make -C Source\C\lcc2 test` also exercises parser, backend and CLI
  integration.
- Explicitly deferred: C's separate scoped `struct` tag namespace and
  forward-tag lookup, along with storage/type forms listed as unsupported in
  `Docs/lcc2_manual.md`. Those limitations are intentional, not incomplete
  work for this task.

### Task 5.3.6 ✅: Peephole optimizer (`peephole.c`)

Added `peephole.c` and integrated it into complete-program generation. 
It parses assembly lines into instruction records while retaining original text for unchanged lines, 
then runs the `Backend.pas` passes in order: legacy constant inc/dec folds, `LIA`/`EXAB`, redundant `PUSH`/`LIB`/`POP`, 
double `EXAB`, and the `INCA`/`DECA` patterns. 
Runtime-library definitions are appended after optimization and remain unchanged; 
`test_runtime_libraries` verifies each comparison helper appears byte-for-byte as its original runtime section 
at the end of generated output. `tests/test_peephole.c` covers each rewrite and representative near-matches; 
`make -C Source\C\lcc2 test` passes, including assembled CLI regression fixtures.

### Task 5.3.7 ✅: Regression tests

The regression suite uses two levels:

1. `tests/test_codegen.c` checks backend output, stack accounting, runtime
   integration, and rejection without partial output for unsupported forms.
   `tests/test_parser.c` checks AST structure, including syntax not yet supported
   by code generation.
2. `tests/test_cli.sh` compiles and assembles representative standard-C
   fixtures. The `pc1403_*.lua` harnesses verify runtime results, stack balance,
   and caller-state preservation on the PC-1403 in MAME; their setup and
   expected results are documented in `Source/C/lcc2/README.md`.

The supported fixtures cover calls, `for`/`continue`, standard `switch`
fallthrough and `break`, structures and members, and pointer reads/writes,
alongside the scalar, array, arithmetic, and logical subsets. The AST parser
accepts some syntax beyond the backend contract; ternary expressions are
regression-tested as parsed ASTs and rejected by code generation. Ternary
evaluation, `enum` objects, and block-scoped typedef declarations remain unsupported per
`Docs/lcc2_manual.md`; CLI regression cases ensure representative uses fail
without replacing prior output. Signed comparisons and arithmetic remain
deferred to Task 5.3.9 rather than treated as a compatibility requirement.

### Task 5.3.8 ✅: Migrate libraries — standard C and device-specific APIs

- Keep the legacy Pascal-compiler headers at the repository root intact; provide
  C headers for `lcc2` under `Source/C/lcc2/include/`.
- Shared infrastructure: external function prototypes use the normal call ABI;
  the optional `--target PC1403` selects external target runtime libraries.
  Keep device selection, ROM addresses, and hardware implementations out of
  codegen. Without `--target`, do not append device-specific runtime code.
- Separate portable library contracts from hardware endpoints and document any
  target-specific departures from standard C behavior.
- Keep library contracts in `Docs/lcc2_libraries.md`, split into standard
  subsets, target-specific functions/adapters, and nonstandard compatibility
  helpers. `Docs/lcc2_manual.md` documents the language and links to this
  separate reference.

#### Task 5.3.8.1 ✅: Standard-C library subset and compatibility helpers

Complete for the explicitly supported subset and accepted device deviations,
not the full standard-C library. Library contracts and migration examples are
in `Docs/lcc2_libraries.md`. The separate LCD/key endpoint work remains in
Task 5.3.8.2.

- Provide minimal `stdint.h` (`uint8_t`, `uint16_t`) and `compat.h` (`byte`,
  `word`) using frontend-supported typedefs. `compat.h` is a legacy
  convenience header, not a standard-C header.
- Supplied `stdio.h` declares `getchar`, `putchar`, and `puts` independently
  of the target. Their implementations are external modules. The accepted
  `puts` contract is deliberately nonstandard: append through NUL without
  newline, return 0 on success or -1 on the first output error, retaining
  any written prefix. Empty strings succeed without a write. This milestone
  does not imply full hosted-C stdio or standard `puts` newline behavior.
- Current foundation: `getchar`/`putchar` declarations and the external
  PC-1403 runtime are implemented as separate, call-selected modules: only
  externally called stdio functions add their runtime, once; declarations
  alone and user-defined implementations do not select target modules.
  `getchar` deliberately returns raw hardware
  key codes, not ASCII, so its current contract is a documented nonstandard
  deviation; do not add a key-to-ASCII translation table.
- Accepted current output contract: PC-1403 `putchar` appends printable ASCII
  bytes to one 24-character line and returns the byte written. A full line or
  unsupported byte (including newline) returns -1 without changing the line.
  No scrolling, wrapping, newline processing, or public reset API is provided.
  The runtime buffer persists across program calls and starts empty on binary
  load. These are documented milestone limitations, not full stream semantics.
  The append-only MAME fixture verified `ABC`, the complete 24-character line,
  return values, full/control-byte rejection, stack balance, register
  restoration, and BASIC responsiveness; scrolling/newline support is deferred.
  `putchar` preserves caller IRAM 8..95 around display preparation and ROM
  output using an 88-byte runtime workspace; otherwise a ROM call could
  destroy the source of a multi-character `puts` operation.
  `puts` selects only its adapter and required `putchar` dependency, once;
  a program-defined `putchar` suppresses the target implementation.
- String subset implemented: `string.h` exposes only standard two-argument
  `strcpy` and `strcat`, returning the destination pointer. Their separate
  device-independent SC61860 runtime modules are selected only for called
  external functions, once, even without `--target`; neither pulls in the
  other. Supported pointer returns and addition of pointee `const` enable
  these signatures. Legacy root headers remain unchanged; the old
  three-argument `strcpy` is not the new API. No other standard string
  functions are in scope. The caller owns termination, capacity and
  nonoverlap; there are no bounds checks or truncation.
- String coverage: actual-header preprocessing, pointer use,
  signature rejection, selective inclusion/deduplication, user-defined
  implementations and assembly; `strings.c` / `strings.lua`
  exercise empty/260-byte strings, high bytes, page crossings, internal/XRAM
  combinations, destination-pointer returns, source/destination guards,
  stack/register preservation and BASIC responsiveness without pausing.
- Supplied `int2str.h` declares the separate nonstandard helper
  `void int2str(unsigned int number, char *destination)`. Its independent,
  call-selected runtime converts 0..65535 to decimal ASCII plus NUL without
  leading zeros, ROM dependencies or static scratch storage. The caller
  provides six writable bytes in internal RAM or XRAM. It is not part of
  string.h or stdio.
- Legacy conversion/display migration is documented and exercised by the
  `libraries.c` fixture: replace legacy `puts.h` with `stdio.h`,
  pass `&buffer[0]` rather than an integer address, and check the output
  result. Root headers and Pascal examples remain unchanged.
- `tests/test_libraries.sh` preprocesses all five supplied headers twice,
  checks typedefs/signatures, independent selection, required dependencies,
  deduplication, user implementations and assembly. The completion harness
  passes 48 distinct vectors in three BASIC `RUN`s: 33 conversion cases and
  15 output cases, including empty/full/control-prefix behavior, page
  crossings, internal/XRAM storage, shared `puts`/`putchar` state and the
  conversion/display example. Stack balance, register restoration and BASIC
  responsiveness are required; MAME remains unpaused.
- The header-using `stdio.c` / `stdio.lua`, append-only,
  input-only and string fixtures retain coverage for raw key input,
  character output, return values and caller-state preservation. Device
  verification is in PC-1403 MAME; physical-device verification is not claimed.

#### Task 5.3.8.2 ✅: Device-specific endpoints — LCD and keyboard

- Migrate `lcd.h` and `key.h` as explicitly nonstandard device APIs, separate
  from `stdio.h` and portable string/conversion helpers.
- Define the LCD operations (`lcd_on`, `lcd_off`, `lcd_cls`, `lcd_pset`,
  `lcd_hline`) and keyboard operations (`key_wait`, `key_scan`, `keybrk`)
  with explicit C signatures and documented hardware-specific semantics.
- Keep PC-1403 implementations, key-code definitions, LCD geometry, ROM
  calls, and memory-mapped I/O in external target libraries. Shared stdio
  adapters may reuse these endpoints without embedding hardware knowledge
  in codegen.
- Raw keyboard endpoints return device key codes; blocking/nonblocking and
  no-key behavior must be explicit. LCD endpoints expose device operations,
  not standard stream semantics.
- Validate target selection and compile/assembly integration, then test
  keyboard scanning/waiting, break-key detection, and LCD clear/pixel/line
  behavior on the selected device, including ABI and caller-state preservation.
- Supplied `lcd.h` and `key.h` declare target-specific endpoints. `key_scan`
  uses the nonblocking `0xFF` no-key sentinel; `lcd_hline` clips at the right
  edge and has a zero-length no-op. Both endpoints and stdio `getchar` use
  external target modules selected only for called functions. Compiler
  selection/assembly checks pass. The `pc1403_device.c` / `pc1403_device.lua`
  MAME fixture passes: 0xFF no-key sentinel, raw scan/wait codes (51/50),
  break press/release, stack balance, caller register restoration and BASIC
  responsiveness; LCD pixel/clipped-line output was checked in LCD memory and
  visually. Device verification is in PC-1403 MAME; physical-device
  verification is not claimed.

### Task 5.3.9 ✅: Port completion gates

- The Pascal reference compiler remains buildable and its existing regression tests continue to pass.
- Every implemented backend capability required by the supported C subset has a contract, codegen, or functional test appropriate to its behavior.
- Unsupported constructs fail with a clear diagnostic; they must not silently generate partial or success-shaped output.
- Complete Task 5.3.9's signed-integer slice and Task 5.3.10's floating-point slice, and do not retire or archive the Pascal compiler until the C compiler satisfies the final acceptance criteria and repository workflows have migrated.

---
# Phase 6 — Advanced optimizations (post-porting)

With an AST available, implement optimizations that the old syntax-directed translator could not do:

### Task 6.1 ✅: Constant folding — evaluate constant expressions at compile time

- Fold side-effect-free constant subtrees for the currently supported
  16-bit unsigned target operations, including arithmetic, bitwise, shifts,
  comparisons, and logical operators. Preserve the target's 255 comparison
  truth value and 0/1 logical truth value; never fold assignments, calls,
  increments, or division by zero. Signed arithmetic semantics remain in
  Task 7.1.
- Implemented in the C compiler's expression walker with shared constant
  evaluation in `symtab.c`. Tests cover wraparound, arithmetic, shifts,
  comparisons, logical/unary results, runtime-side-effect preservation, and
  rejection of division by zero. The full `make test` suite passes.

### Task 6.2 ✅: Reduce generated code for byte operations and expressions

Target the code-size overhead in the current `lcc2` byte-expression path while
preserving C value semantics and the existing call ABI. Keep this work within
the current unsigned-integer subset; signed promotion and comparison behavior
remain in Task 7.1.

#### Task 6.2.1 ✅: Avoid materializing discarded assignment results

- First optimize discarded assignments to directly resolved scalar identifiers
  (register, XRAM, and local): evaluate the right-hand side once and store it
  without reloading the assigned value. Keep assignments through fixed-index
  arrays, structure members, or pointers on their existing paths in this step.
- Preserve the current value-producing path wherever an assignment result is
  consumed, including nested assignments and comma expressions. Do not change
  side-effect ordering or evaluate an operand more than once.
- Test discarded byte and word assignments, side-effecting right-hand sides,
  nested assignments whose values are consumed, and assignments inside comma
  expressions. Verify emitted sequences and stack accounting.
- Measure the bounce image before and after this step. **Complexity: Low;
  estimate 0.5–1 engineering day.** Discarded expressions are already
  identified by `emit_discarded`; this first slice should remove the redundant
  scalar reload without changing value-producing expressions.
- Implemented the direct-identifier scalar path in `emit_discarded`, preserving
  the value-producing path for nested assignments and leaving array/member/
  pointer stores unchanged. Codegen tests cover register, XRAM, and local
  byte/word destinations, side-effecting calls, consumed nested results, comma
  expressions, and stack balance; `make test` passes. The
  `discarded_assignments.c` MAME test passes for ten BASIC calls, checking
  discarded stores, consumed nested/comma results, exactly-once RHS calls,
  and BASIC memory integrity. The bounce image fell from 3,347 to 3,207 bytes
  (140 bytes, 4.2%).

#### Task 6.2.2 ✅: Generate width-aware byte expressions

- For value-producing assignment to an unsigned XRAM byte, reuse the converted
  B:A result instead of reloading the stored byte. If the RHS is not already a
  known unsigned byte, clear B after evaluating it to apply the assignment's
  conversion before storing.
- Keep other destinations and indirect stores on their existing paths in this
  slice. Then expand to other byte operations only where resolved types and C
  integer promotions prove the narrower sequence equivalent. Preserve
  word/pointer behavior and do not introduce signed semantics, which remain in
  Task 7.1.
- Test byte and word operations, unsigned wraparound, mixed-width expressions,
  assignment results, and contexts that require promotion. Compare generated
  assembly and run the relevant target-level fixtures.
- First conservative slice implemented in `codegen.c`: a known unsigned-byte
  RHS assigned to an unsigned XRAM byte keeps its zero-extended B:A result,
  avoiding the redundant reload. Codegen coverage and the
  `discarded_assignments.c` MAME test pass (ten BASIC calls); its image shrank
  from 244 to 238 bytes. The bounce image remains 3,207 bytes because it has no
  matching value-producing XRAM byte assignment.
- Second slice extends this to wider or signed RHS values: clear B after
  evaluation to retain the converted unsigned-byte result, then store without
  reloading. Keep further expression classes deferred until their promotion
  and conversion semantics are separately proven. The updated runtime fixture
  now covers narrowing `0x12A5` to an unsigned byte and using the resulting
  `0x00A5` assignment value; MAME passes ten BASIC calls. The same fixture
  shrank from 277 to 273 bytes (4 bytes), while bounce remains unchanged.

#### Task 6.2.3 ✅: Specialize comparisons for byte operands

- Emit direct comparisons for compatible unsigned byte operands instead of
  routing every comparison through the 16-bit helpers. Preserve the current
  truth-value convention and use the existing word path for wider or mixed
  operands unless a valid narrowing conversion is established.
- Test equality and relational boundaries, including 0, 255, mixed byte/word
  operands, and byte values used in conditions. Signed comparisons remain in
  Task 7.1.
- Implemented direct compare/branch sequences only when both operands are
  resolved as unsigned bytes. All six operators preserve the established
  `0`/`255` result, while mixed-width and other comparisons retain the
  16-bit-helper path. Codegen tests assert both paths. The PC-1403 MAME
  comparison fixture passes 130 calls, covering the six byte operators at
  `0`, `1`, `254`, and `255` boundaries alongside the existing word vectors;
  stack balance and register restoration pass. `make test` and assembly pass.
  A six-operator byte fixture assembles to 299 bytes versus 413 bytes for the
  equivalent word fixture (114 bytes, 27.6% smaller); these are standalone
  comparison measurements, not a bounce-image change.

#### Task 6.2.4 ✅: Measure generated-code changes

- For each optimization step, record assembled image sizes for the bounce
  demo and representative byte/word fixtures. Keep existing codegen, CLI,
  assembly, and runtime tests passing; report size changes separately from
  behavior changes.
- Rebuilt and assembled the lcc2 bounce demo and the legacy Pascal bounce demo
  with PASM. At the end of the Task 6.2 backend work, the lcc2 image was 3,191
  bytes, down 156 bytes (4.7%) from the 3,347-byte global-state version before
  Task 6.2; the Pascal image is 1,509 bytes.
- A subsequent PC-1403 LCD runtime change shares the endpoints' 88-byte
  caller-register save area. The current bounce image is 2,927 bytes (264
  bytes smaller than 3,191), and the all-endpoint device fixture is 1,415
  bytes (352 bytes smaller than its 1,767-byte pre-change image). The current
  bounce image is 1.94 times the 1,509-byte Pascal image.

------
# Phase 7 — Cleanup, docs and release

The porting to C has reached a sufficient maturity, to consider
the replacement to the legacy (Pascal-based) lcc virtually complete.

Phases 8 and 9 are further optimizations, and feature enrichments.

### Task 7.1: Review the user manual (`Docs/lcc2_manual.md`).

Keep it simple and up-to-date with the current supported subset, and add a "known limitations" section.
Add reference to practical demo and examples
    
### Task 7.3: Update README with build instructions (`make` / `cmake`).

### Task 7.4: Publish release with prebuilt binaries (Windows, Linux).

Verification: build + full test suite green on Windows and Linux.

------
# Phase 8 — Further optimizations

### Task 8.1: Dead code elimination

Implement as two independently testable `lcc2` code-generation optimizations.
Estimates below include focused regression tests and assume the current
unsigned-integer subset; they exclude signed semantics (Task 7.1) and target
runtime testing.

#### Task 8.1.1: Constant-condition branch elimination

- After the translation unit has passed normal parsing and semantic
  resolution, use `symtab_fold_constant` to select the live branch of an
  `if` whose condition is a side-effect-free constant. Omit the `if` and its
  condition evaluation, emitting only the selected branch; support both
  `if (0)` and nonzero constant conditions. Leave nonconstant conditions
  unchanged.
- Preserve diagnostics: dead-branch contents must still be parsed and
  semantically validated, and an invalid construct must not become accepted
  merely because it is unreachable. Use only the existing constant-folding
  semantics; signed-condition behavior remains subject to Task 7.1.
- Test zero/nonzero conditions, nested conditionals, dangling `else`, side
  effects in nonconstant conditions, and invalid code in a dead branch.
  Verify both emitted assembly and that runtime behavior of retained branches
  is unchanged.
- **Complexity: Low; estimate 0.5–1.5 engineering days.** The constant
  evaluator and semantic-validation-before-emission path already exist; the
  main work is careful statement emission and regression coverage.

#### Task 8.1.2: Unreachable function elimination

- For complete-program generation, build reachability from the required
  `main` definition by walking direct calls in reachable function bodies.
  Emit reachable definitions only; retain existing call behavior and
  declarations/prototypes needed for resolution. Handle recursion and cycles
  by marking each function before traversing its body.
- The call-graph walk must cover calls in expressions, initializers, and all
  supported statement forms. Where an inline-assembly statement could hide
  a function reference, conservatively retain all definitions rather than
  risk removing a referenced routine. If constant-condition elimination is
  active, the walk should follow only the selected branch for foldable `if`
  conditions.
- Test direct and transitive reachability, unused functions, recursive and
  mutually recursive functions, prototypes, calls in nested/control-flow
  expressions, inline-assembly fallback, and unchanged runtime-library
  inclusion. Check that the ordinary translation-unit code-generation API
  retains its existing behavior unless its contract is explicitly changed.
- **Complexity: Medium; estimate 2–4 engineering days.** The AST and direct
  call support are in place, but a complete AST walk, conservative handling
  of opaque assembly, and integration with complete-program section emission
  require broader coverage than branch pruning.

### Task 8.2: Register allocation — smarter use of registers I, J, K, L, M, N
### Task 8.3: Strength reduction — `x * 2` → `x << 1`, `x * 3` → `(x << 1) + x`
### Task 8.4: Tail call optimization — useful given the very small stack (~80 bytes)
### Task 8.5: Inline expansion — inline small functions (1–3 instructions)

---
# Phase 9 — Advanced arithmetics

### Task 9.1: Signed integer semantics

- Correct signedness, integer promotions/conversions, and comparisons for
  supported signed and unsigned byte/word expressions without changing
  legacy unsigned behavior; replace the current "signed relational comparison
  is not supported" rejection with real signed comparisons (e.g. `SBB`-based).
- Extend the Task 5.3.4.12 operators (division, remainder, shifts, unary
  minus) and Task 5.3.4.13 `switch` to signed operands.
- Cover arithmetic results, boundary values (-1 vs 1, 0x7FFF/0x8000, signed
  bytes), compound uses, and byte promotion.
- Add resolver/codegen tests and target-level functional tests for negative
  values and signed/unsigned comparison boundaries.
- Until this task is complete, signed operations beyond the currently
  supported forms must continue to fail explicitly.

### Task 9.2: Floating-point support (deferred until after Task 5.3.9)

- Add floating-point expression, conversion, and storage behavior only after
  the standard-C library headers in Task 5.3.8 have been migrated.
- Establish the supported precision/ABI and target-specific runtime-library requirements
  before implementation; test constants, arithmetic, comparisons, calls, and
  target execution. 
- Until this task is complete, floating-point constructs must continue to
  fail explicitly and must not be treated as supported integer expressions.

### Task 9.3: review optimizations

- Review the optimizations introduced earlier, after new arithmetic and signed/unsigned operations have been implemented, 
  and ensure they are correct and complete.

---

# Riepilogo fasi e dipendenze

```
Phase 0-3  ✅  Completate (modularizzazione Pascal)
Phase 4    🔧  In corso (separazione sintassi ↔ emissione)
  └─ Task 4.1.14: completare la migrazione dell'emissione ASM residua da parser.pas

Phase 5    📋  Migrazione a Flex/Bison
  ├─ Phase 5.1: specifica C, AST tipizzato e contratto di compatibilità CodeGen
  │   ├─ 5.1.1: specifica sottoinsieme C per SC61860
  │   ├─ 5.1.2: definire nodi AST (C standard)
  │   ├─ 5.1.3: grammatica BNF formale (formato Bison)
  │   └─ 5.1.4: inventario API CodeGen + fixture di parità
  ├─ Phase 5.2: estrazione/test peephole Pascal come ponte verso il porting C
  │   ├─ 5.2.1: estrarre ottimizzazioni da SecondScan
  │   └─ 5.2.2: test unitari per i pattern peephole
  └─ Phase 5.3: nuovo compilatore Flex/Bison (C), backend portato a milestone
      ├─ 5.3.1: setup progetto C
      ├─ 5.3.2: lexer Flex
      ├─ 5.3.3: parser Bison
      ├─ 5.3.4: backend SC61860 tree-walker
      ├─ 5.3.5: symbol table con scope annidati
      ├─ 5.3.6: peephole optimizer
      ├─ 5.3.7: regression test + test nuovi costrutti
      ├─ 5.3.8: migrazione header libreria
      ├─ 5.3.9: semantica signed (dopo migrazione header)
      ├─ 5.3.10: supporto floating-point (dopo 5.3.9)
      └─ 5.3.11: gate di completamento porting

Phase 6    📋  Ottimizzazioni avanzate (AST-based)
Phase 7    📋  Cleanup e rilascio
```

**Dipendenze critiche:**
```
Task 4.1.14 ──→ Phase 5.1 ──→ Phase 5.2 ──→ Phase 5.3 ──→ Phase 6 ──→ Phase 7
                │                            │
                └── 5.1.3 (BNF) ─────────────→ 5.3.3 (Bison)
                └── 5.1.2 (AST) + 5.1.4 (CodeGen contract/fixtures) ──→ 5.3.4 (backend port)
```

---

# Main risks and mitigations

| # | Risk | Impact | Mitigation |
|---|------|--------|------------|
| 1 | Il nuovo backend diverge da sequenze SC61860 già collaudate in `CodeGen.pas` | Alto | Inventariare le API e aggiungere fixture di parità prima del porting; portare per gruppi |
| 2 | Stato globale Pascal (`pushcnt`, label, librerie, output) viene replicato implicitamente in C | Alto | Riunire lo stato in `CodegenContext` e testare stack/lib state per funzione |
| 3 | Programmi littleC esistenti non compilano col nuovo frontend C | Medio | Fornire `compat.h` e una guida di migrazione; mantenere suite di compatibilità |
| 4 | Semantiche C standard divergono dall'output littleC | Medio | Separare test di parità backend da test funzionali per cambiamenti intenzionali |
| 5 | `struct` richiede calcolo offset e gestione alignment | Medio | Definire layout esplicito; validare sul target SC61860 |
| 6 | `signed` arithmetic richiede implementazione non presente nel backend | Medio | Aggiungere e testare le operazioni/librerie solo quando richieste dal subset |
| 7 | Peephole fragile se il formato dell'output cambia | Basso | Estrarre prima il comportamento Pascal; testare ordine e casi non corrispondenti prima di portarlo |
| 8 | Flex/Bison non disponibili facilmente su Windows | Basso | `winflexbison` (pacchetto precompilato), oppure build in WSL |

---

# Practical development suggestions

-   Completare Phase 4 (nessuna emissione ASM sintattica residua nel parser) è il **prerequisito critico** per il nuovo frontend. Output serializzato e diagnostica restano fuori da tale obiettivo.
-   Prima del codice C, catalogare l'API effettiva di `CodeGen.pas` e scegliere fixture rappresentative; non serve ricostruire un AST Pascal.
-   Portare il backend C incrementalmente seguendo i costrutti implementati e verificare ogni slice contro il contratto C e le fixture funzionali.
-   Per Phase 5.3, partire da un **micro-compilatore C esistente** come reference architetturale:
    -   **chibicc** (Rui Ueyama): ~5000 righe, ottima documentazione, C11 quasi completo, molto didattico.
    -   **8cc** (stesso autore): predecessore di chibicc, più piccolo.
    -   **lcc** (Fraser & Hanson): compilatore C accademico con backend retargetable — il più vicino concettualmente.
    -   **cproc** (Michael Forney): ~6000 righe, C11, backend QBE — buon esempio di struttura.
-   Usare `test.bat` per le fasi Pascal; per le fasi C, creare un `Makefile` con target `test` equivalente.
-   I typedef di convenienza (`byte`, `word`) in `compat.h` permettono di compilare gran parte dei demo esistenti con modifiche minime.

---

# Quality checklist (Quality Gates)

**Fasi 4-5.2 (Pascal):**
-   Build: FPC compila senza errori.
-   Regression: `test.bat` → NO_DIFF su tutte le demo.
-   Nessuna emissione ASM diretta nel parser (da Phase 5 in poi).
-   Le trasformazioni peephole hanno test positivi e negativi, e preservano output non coinvolto.

**Fasi 5.3+ (C):**
-   Build: `make` compila senza errori né warning (`-Wall -Wextra`).
-   Test di parità: le operazioni portate da `CodeGen.pas` conservano sequenza e formato ASM sui fixture equivalenti.
-   Test funzionali: ogni demo supportata produce ASM assemblabile con `pasm` e funzionalmente corretto.
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