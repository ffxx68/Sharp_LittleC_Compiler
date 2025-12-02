# Refactor roadmap for `lcc` (SC61860)

This document collects the analysis and the refactor plan to make the `lcc` compiler located in `Source/lcc` more modular, readable and maintainable.

## Executive summary
- Goal: separate responsibilities (lexing, parsing, symbol table, semantic, codegen, output, optimizations, utilities) into distinct Pascal units, keeping backward compatibility and allowing incremental, verifiable refactors.
- Approach: incremental refactor in phases, running tests and compilation after each phase, using compatibility wrappers to preserve public APIs.

---

## Initial checklist (to complete)
- [x] Create a git branch `refactor_modular` (recommended).
- [x] Run a baseline build (compile `Source/lcc`) and save the output/log.
- [x] Create this roadmap file (this file).

---

## Build environment notes
- Lazarus is installed in: `C:\Users\F.Fumi\lazarus\`
- The compiler is: `$(Lazarusdir)\fpc\3.2.2\bin\i386-win32\fpc.exe`
- Baseline build log: `Source/lcc/baseline_build.log`

---

## Test Scripts

- **test.bat**: Script per testare la toolchain lcpp + lcc su un sorgente di test (es. test_C/bounce/main.c), preprocessando e compilando il file, con output in test_C/bounce/tmp.asm.

---

## Current component map (high-level)
- `input.pas`: character input, `GetChar`, global variable `Look`.
- `scanner.pas`: tokenization (GetToken, GetName, GetNumber, GetFloat), string parsing utilities, input modes (file/string/keyboard), global variables `Tok`, `dummy`, `Level`, `md`, `linecnt`.
- `parser.pas` / `parser_new.pas`: parsing, symbol table (VarList/ProcList), semantic actions (AddVar/AddProc/AllocVar/repadr), direct asm generation, post-process optimizations.
- `CodeGen.pas`: label generation, helpers varxram/varcode/varreg, operation helpers (CompEqual, CompGreater, ...), library handling.
- `output.pas`: accumulation of `asmtext`/`libtext`/`asmlist`, `writeln` wrapper and `addasm` functions.
- `errors.pas`: Error/Expected that use scanner globals for context.
- `calcunit.pas`: Evaluate() for evaluating mathematical expressions.

---

## Main issues identified
- Parsing, semantic analysis and code emission are mixed in the same file (`parser.pas`).
- Heavy use of global variables shared between units (Look, Tok, dummy, linecnt, pushcnt, optype...).
- Duplication between `parser.pas` and `parser_new.pas`.
- Post-processing/optimizations are mixed into `SecondScan`.

---

## Target components (proposed)
For each component I list responsibilities, minimal API, dependencies and priority.

1) `Lexer` (extracted from `scanner.pas`)
- Responsibility: lexing/tokenization, string utilities strictly necessary for the lexer.
- Minimal API: `InitFromFile`, `InitFromString`, `GetToken(mode, var s)`, `GetName`, `GetNumber`, `GetFloat`, `CopyToken`, `CurrentToken`, `CurrentLine`.
- Dependencies: `Input`, `Errors`.
- Priority: High.

2) `SymbolTable` (extracted from parser VarList/ProcList)
- Responsibility: management of VarList/ProcList, Find/Add/Alloc/IsVarAtAdr/RemoveLocalVars.
- Minimal API: `FindVar`, `AddVar`, `AllocVar`, `FindProc`, `AddProc`, `RemoveLocalVars`, getters for metadata.
- Dependencies: `CalcUnit` (for mathparse), `Errors`.
- Priority: High.

3) `Semantic` (vardecl / repadr / checks logic)
- Responsibility: var-declaration parsing helpers, repadr, type/overlap validations.
- Minimal API: `ParseVarDecl(tok: string): string` (or similar wrapper), `RepAdr(currproc)`.
- Dependencies: `SymbolTable`, `CalcUnit`, `Errors`.
- Priority: High.

4) `CodeGen` (refactor)
- Responsibility: instruction emission, label management, library handling, varXxx helpers.
- Minimal API: `NewLabel`, `PostLabel`, `EmitInst`, `AddLib`, `VarXram`, `VarCode`, `VarReg`, `Flush`.
- Dependencies: `Output`.
- Priority: High.

5) `Output` (clean)
- Responsibility: accumulate `asmtext`/`libtext`/`asmlist`, write to file.
- Minimal API: `Emit`, `AddAsm`, `SaveToFile`, `Reset`.
- Priority: High.

6) `Backend` (optimizations and post-processing)
- Responsibility: transformations of `asmtext` (pattern replace), instruction compression, steps currently in `SecondScan`.
- Minimal API: `OptimizeAsm(asmText): string`, `WriteAsmFile(filename, asmText, libText)`.
- Priority: Medium-High.

7) `CalcUnit` (clean up)
- Responsibility: `Evaluate()` for expressions and hex/bin conversions.
- Minimal API: `Evaluate`, `ConvertHex`, `ConvertBin`.
- Priority: Medium.

8) `Errors` (improve)
- Responsibility: error reporting with context (line, token, dummy)
- Minimal API: `Error(msg)`, `Expected(msg)`, `Warning(msg)`.
- Priority: Medium.

---

## Concrete ordered tasks (incremental iterations)
Each item is designed to be small and verifiable.

Phase 0 — Preparation (0.5 day)
- [x] Task 0.1: create a git branch for the refactor.
- [x] Task 0.2: run baseline build (compile `Source/lcc`) and save the output.
- [x] Task 0.3: generate and save reference ASM (copy original `tmp.asm` -> `Source/lcc/reference_bounce.asm`) — used as regression reference for future tests.

Phase 1 — Extract `Lexer` (2 days)
- [x] Task 1.1: create `Lexer.pas` that re-exports the public functions from `scanner.pas` (GetToken, GetName...).
- [x] Task 1.2: replace direct uses in the `parser` with `Lexer` (use wrappers to keep compatibility).  
  - Done: updated `parser.pas` to call `Lexer.Get*` wrappers where needed and verified by running the `bounce` demo; generated `tmp.asm` matches `reference_bounce.asm` (NO_DIFF).
- [x] Verification: build OK, run tokenization test on a demo file (generated `test_C\bounce\tmp.asm` matches `reference_bounce.asm`).

Phase 2 — Extract `SymbolTable` and `Semantic` (3 days)
- [x] Task 2.1: create `SymbolTable.pas` and move VarList/ProcList, Find/Add/Alloc. (COMPLETED)
  - [x] Created initial `Source/lcc/SymbolTable.pas` (minimal stub) and then a fuller implementation with Var/Proc API (FindVar, AddVar, AllocVar, GetVar/GetProc accessors).
  - [x] Added overloaded compatibility API `IsVarAtAdr(adr,size; out foundIdx)` plus a two-arg wrapper for backwards compatibility.
  - [x] Replaced caller sites in `Source/lcc/parser.pas` for variable-address queries to use `SymbolTable.IsVarAtAdr` (incremental, targeted replacements only — kept backup of original file).
  - [x] Added `Source/lcc/symboltest.pas` and verified it compiles (`SYMBOLTEST_OK`).
  - [x] Ran full demo test (build + lcpp + lcc) for `test_C/bounce` — result: `NO_DIFF` (no regression on generated asm).
  - [x] Migrated `FindVar` and `FindProc` to use `SymbolTable` API (wrapper functions).
  - [x] Added synchronization in `AddVar` and `AddProc` to keep `SymbolTable` in sync with local arrays.
  - [x] Created conversion functions `VarEntryToTVarInfo` and `ProcEntryToTProcInfo` for type compatibility.
  - [x] Migrated read accesses to `VarList[i]`/`ProcList[i]` to use `GetVarInfo(i)`/`GetProcInfo(i)` in print functions and initialization loops.
  - [x] Migrated `VarList[VarFound]`/`ProcList[ProcFound]` accesses to use `GetVarInfo(varfound)`/`GetProcInfo(procfound)`.
  - [x] Verified all changes with `test.bat` — result: `NO_DIFF` (no regression).
  - Note: Local arrays `VarList`, `ProcList`, `VarCount`, `ProcCount` still exist in `parser.pas` for write operations during parsing. These will be fully removed in a future refactor phase.
- [x] Task 2.2: create `Semantic.pas` for vardecl/repadr/checks. (COMPLETED)
  - [x] Created `Source/lcc/Semantic.pas` with `VarDecl` and `RepAdr` functions.
  - [x] Added `SetVarAddress(idx, newAddress)` to `SymbolTable.pas` API for updating variable addresses.
  - [x] Migrated `RepAdr` function to use `SymbolTable` API (`GetProcInfo`, `FindVar`, `SetVarAddress`).
  - [x] Updated `parser.pas` to delegate to `Semantic.RepAdr` and sync back to local arrays for compatibility.
  - [x] Verified with `test.bat` — result: `NO_DIFF` (no regression).
- [x] Verification: `FirstScan` produces var/proc tables identical to the baseline.

Phase 3 — Refactor `CodeGen` + `Output` (3 days)
- [>] Task 3.1: add `EmitInst` in CodeGen and use `Output.Emit` instead of direct `writeln`. (95% COMPLETE - final verification pending)
  - [x] Created `EmitInst` family of functions in `CodeGen.pas`:
    * `EmitInst(inst)` - single instruction
    * `EmitInst(inst, operand)` - instruction with operand
    * `EmitInst(inst, operand, comment)` - instruction with operand and comment
    * `EmitComment(comment)` - standalone comment
    * `EmitBlankLine` - blank line
  - [x] **parser.pas migrations** (~300+ writln/writeln calls converted):
    * Store: ALL variants (byte/char/word/float + arrays, register/XRAM, local/global)
    * LoadConstant, LoadVariable: ALL data types, modes, and arrays
    * Factor: address-of operator (&), procedure calls, pointer dereferencing
    * Assignment: ALL assignment types including pointer assignments
    * Control flow: DoIf, DoWhile, DoLoop, DoFor, DoDoWhile, Switch, DoGoto, DoBreak, DoReturn
    * DoLoad, DoSave: Added missing procedures
    * ProcCall: COMPLETE (parameters, locals, stack management) - migrated to CodeGen.EmitComment/EmitBlankLine
    * Block: procedure returns, inline assembly (#asm blocks) - migrated end blank line to CodeGen.EmitBlankLine
    * DoLabel: migrated to PostLabel + CodeGen.EmitComment
    * Increment/decrement operators (++/--)
  - [x] **CodeGen.pas partial migrations** (~30 writln calls converted):
    * load_x, load_y helpers
    * varxram, varxarr (byte/char small arrays)
  - [x] **SecondScan fixes**: ALL `writeln()` now write to file with `writeln(f, ...)` instead of console
    * Fixed intro section, registry save/restore
    * Fixed procedure loop output
    * Fixed asmlist output
  - [x] **Console output cleanup**: NO assembly code on console anymore! ✅
  - [x] All tests passing with **NO_DIFF** - zero regressions!
  - [~] **Status: Almost complete, final verification needed**:
    * Reported by user: still ~11 `writln` occurrences in parser.pas (need verification)
    * Note: inline assembly (#asm blocks) still uses 1 `writln` - intentional, writes to asmtext buffer
    * Remaining ~150 `writln` in CodeGen.pas deferred to incremental future refactoring (still functional)
  - [ ] **Next step**: Final audit of all `writln` in parser.pas to ensure 100% migration or document intentional uses
- [ ] Task 3.2: consolidate `addlib` and `libtext` handling into `CodeGen` -> `Output`.
- [~] Verification: `SecondScan` generates functional asm; build OK. ✅ (Passing - final writln audit pending)

Phase 4 — Reduce Parser: separate syntax from emission (4-6 days)
- [ ] Task 4.1: replace inline asm generation with calls to `CodeGen.Emit*`.
- [ ] Task 4.2: consider introducing an AST (optional) for complex expressions.
- [ ] Verification: generated asm is semantically identical; tests on demos pass.

Phase 5 — Backend and optimizations (2-3 days)
- [ ] Task 5.1: extract optimization logic (temp -> temp2 passes) into `Backend.pas`.
- [ ] Task 5.2: create tests for optimizations (input with known pattern => expected output).
- [ ] Verification: optimizations preserve equivalence and do not introduce regressions.

Phase 6 — Hardening, cleanup, docs and tests (2 days)
- [ ] Task 6.1: remove duplicates (`parser` vs `parser_new`) after verification and consolidation.
- [ ] Task 6.2: update README, add small scripts/CI for automated tests.
- [ ] Verification: build + smoke tests green.

Estimated total: ~14-18 working days.

---

## Main risks and mitigations
- Risk: API breakage because many functions rely on globals — Mitigation: create identical wrappers and move functionality progressively.
- Risk: Differences between `parser.pas` and `parser_new.pas` — Mitigation: compare outputs using a test suite and choose the best base.
- Risk: optimizations rely on textual matching and are sensitive to formatting/whitespace — Mitigation: implement documented, testable transformation functions.

---

## Practical development suggestions
- Make frequent, small commits for each extraction ("extract Lexer", "extract SymbolTable").
- Add a small harness that runs `lcc` over a list of demos and compares the generated asm with reference files (regression tests).
- Use wrappers to preserve existing public calls (e.g., `GetToken`) and then deprecate the old files gradually.

---

## Quality checklist (Quality Gates)
- Build: compile the whole `Source/lcc` after each phase — PASS.
- Lint/Typecheck: check warnings and resolve them — preferably PASS with no critical warnings.
- Unit tests: add tests for `Lexer`, `SymbolTable`, `Backend.OptimizeAsm` — PASS.
- Smoke tests: run `lcc` on 3-5 demos and check the asm is assemblable — PASS.

---

## Acceptance criteria
- All new units compile without errors with FPC/Delphi.
- The external behavior of `lcc` on reference demos remains equivalent (assembler output is assemblable with `pasm`).
- The `parser` no longer emits asm strings directly but calls `CodeGen`/`Output` (intermediate target).
- Documentation updated (`Source/lcc/README.md` or this `refactor_roadmap.md`).

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
1. Confirm I can create the branch and start implementing `Lexer.pas` as a wrapper of `scanner.pas` (I can run and compile afterwards).
2. Alternatively, I can immediately generate the stub files (`Lexer.pas` and `SymbolTable.pas`) and perform a minimal compilation test.

---

File created automatically: `Source/lcc/refactor_roadmap.md`.

---