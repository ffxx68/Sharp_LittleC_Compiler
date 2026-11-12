# PASM (Assembler) — Tests and Verification

This directory contains the C version of the PASM assembler and a test suite for binary comparison with the original Pascal implementation.

Files of interest
- `pasm.c`, `pasm.exe` (local build)
- `test/test_pasm.ps1` — PowerShell script that runs both the C and Pascal versions of PASM (the latter is expected at `..\..\..\pasm.exe`) and compares their binaries.
- `test/test_simple.asm` — minimal test: LIA, LIB, STP, RTN
- `test/test_complicated.asm` — extended test: opcodes, `.DB`, labels, jumps, calls, expressions, and the `EQU` directive
- `test/test_complicated_2.asm` — extended test with additional features
- `test/test_complicated_3.asm` — test with many instructions and edge cases
- `test/test_case.asm` — `CASE`/`ELSE`/`ENDCASE` tables with forward references and labels whose names are prefixes of already-defined labels

## Building on Windows and Linux

GCC and GNU Make are required. From the `Source/C/pasm` directory, run:

```sh
make
```

On Windows, with GNU Make and GCC (for example, in an MSYS2/MinGW shell), the
target is `pasm.exe`. On Linux, the target is `pasm`, with no file extension.
To compile directly without Make:

```sh
gcc -Wall -Wextra -O2 -std=c99 -o pasm pasm.c
```

Run `make clean` to remove the generated executables.

How the test script works
- Run `test/test_pasm.ps1` from the `test` subdirectory, passing the `.asm` filename as a parameter. The script:
  - assembles it with the local C version of PASM, producing `<name>_c.bin` (optionally use `-d` to generate `debug.txt`),
  - assembles the same `.asm` file with the Pascal version of PASM, producing `<name>_pascal.bin`,
  - displays the hex dumps and compares the binaries (`fc /b`).

PowerShell command examples

```powershell
cd test
.\test_pasm.ps1 test_simple.asm
.\test_pasm.ps1 test_complicated.asm
.\test_pasm.ps1 test_complicated_2.asm
.\test_pasm.ps1 test_complicated_3.asm
```

## Test Status

**Tests run and passed:**
- `test_simple.asm` — PASS
- `test_complicated.asm` — PASS
- `test_complicated_2.asm` — PASS
- `test_case.asm` — PASS

**Failed test:**
- `test_complicated_3.asm` — FAIL (many differences between the C and Pascal binaries)

**Notes:**
- The C version produces binaries identical to the Pascal version for the first three tests, as verified with `fc /b`.
- `CASE`/`ENDCASE` tables are supported as in Pascal PASM: `PTC` with the count and end-of-table address patched at `ENDCASE`, `ETC`, `value label` entries, and `ELSE label` entries (address only). An `ELSE` with a forward-referenced label is patched to that label's address (the same fix was applied to `Source\pasm\pasm.dpr`).
- `calcadr` recalculates the parameter length after replacing a resolved label and no longer discards forward references whose names are substrings of already-defined labels (for example, `CPS1` after `LIB_CPS16`).
- All `Source\C\lcc2\build\*.asm` fixtures produce binaries identical to those from Pascal PASM; `pc1403_switch`, assembled with C PASM, passes the MAME test.
- Label handling and the `.DW`, `.DB`, and `.ORG` directives have been aligned with the Pascal version and validated.
- **Still to be verified:**
  - The `.EQU` directive (symbol definition)
- `test_complicated_3.asm` still produces many binary differences; analysis is ongoing.

---
Last updated: 2025-11-25
