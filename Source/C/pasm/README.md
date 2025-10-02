# PASM Migration from Pascal to C

## Project Overview

This project documents the migration of the **PASM** assembler from the original Pascal language to C, with the goal of maintaining full functional compatibility and identical binary output.

**Last modified:** October 2, 2025

## Current Status - 🔄 VALIDATION IN PROGRESS

### ⚠️ Validation Tests Ongoing

- The migration of the codebase is complete and initial validation tests are passing
- **Two test cases** have been successfully verified with binary-identical output
- **Additional testing required** for complex features and edge cases
- The assembler should NOT be considered fully production-ready until all validation steps are complete

## Compatibility Verification - ✅ PARTIAL SUCCESS

### Tests Performed

#### Test 1: Basic Directives and Backward Jumps ✅ PASSED
- **Test file:** `test_org_equ.asm`
- **Binary comparison:** `fc.exe /b test_org_equ_corrected.bin test_org_equ_pascal.bin`
- **Result:** **IDENTICAL byte by byte** ✅

#### Test 2: Forward Jumps and Label Resolution ✅ PASSED
- **Test file:** `test_jump_forward.asm`
- **Features tested:** Forward jump (`JRP NEXT`), label resolution, etichette non ancora definite
- **Binary comparison:** `fc.exe /b test_jump_forward_c.bin test_jump_forward_pascal.bin`
- **Result:** **IDENTICAL byte by byte** ✅
- **Debug output:** File `debug.txt` generato correttamente con traccia di risoluzione etichette

#### Test 3: Extended Features ⏳ PENDING
- **Test file:** `test_extended_features.asm`
- **Status:** **NOT YET TESTED**
- **Features to verify:** Complex assembly constructs, edge cases, advanced directives

### Verified Features
- ✅ Handling of `.ORG 40000` and `.EQU regB 3` directives
- ✅ Assembly of `LIA regB` instructions (02 03)
- ✅ Relative jumps `JRM START` (2D 03)
- ✅ Forward jumps `JRP NEXT`
- ✅ Forward label resolution
- ✅ Symbol and label resolution
- ✅ Generation of binary file identical to Pascal
- ✅ Debug mode (-d flag)

### Features Still to Verify
- ⏳ Extended assembly features in `test_extended_features.asm`
- ⏳ Complex edge cases and error conditions
- ⏳ All demo files compatibility

## File Structure

```
Source/C/pasm/
├── pasm.c                      # Main assembler C file
├── pasm_constants.h            # Header with protected OPCODE/NBARGU arrays
├── pasm.exe                    # Generated C executable
├── debug.txt                   # Debug output (when -d flag used)
├── test_org_equ.asm            # Assembly test file (backward jumps)
├── test_org_equ_pascal.bin     # Pascal reference output
├── test_org_equ_corrected.bin  # Verified identical C output
├── test_jump_forward.asm       # Assembly test file (forward jumps)
├── test_jump_forward_c.bin     # C version output
├── test_jump_forward_pascal.bin # Pascal reference output
├── test_extended_features.asm  # Complex features test ⏳ PENDING
└── README.md                   # This file
```

## Debug Features

### Debug Mode
- **Command line flag:** `-d` o `--debug`
- **Output file:** `debug.txt`
- **Features:**
  - Traccia completa del processo di risoluzione etichette
  - Debug di salti relativi e calcolo offset
  - Informazioni dettagliate su operazioni JR/JRPLUS

## Next Steps - 🔄 TESTING REQUIRED

### Pending Tasks
- ⏳ **Test extended features** with `test_extended_features.asm`
- ⏳ **Verify complex assembly constructs**
- ⏳ **Test edge cases and error handling**
- ⏳ **Validate all demo files**

### Optional Future Enhancements
- [ ] Performance benchmarking vs Pascal version
- [ ] Extended error reporting and validation
- [ ] Integration with build system

## Current Status Summary

**The PASM C migration is FUNCTIONALLY COMPLETE but requires additional validation.** 

✅ **Working and verified:**
- Basic assembly instructions
- Backward relative jumps (JRM)
- Forward relative jumps (JRP)  
- Label resolution (both forward and backward)
- Basic assembler directives (.org, .equ)
- Debug mode functionality

⏳ **Still to be tested:**
- Extended features and complex constructs
- Edge cases and error conditions
- Full demo compatibility

---
*Status as of: October 2, 2025 - Partial validation completed, extended testing pending*
