# PASM (Assembler) — Tests and Verification

Questo folder contiene la versione C di PASM assembler e una suite di test per la verifica binaria rispetto all'implementazione Pascal originale.

File di interesse
- `pasm.c`, `pasm.exe` (build locale)
- `test/test_pasm.ps1` — Script PowerShell che esegue sia PASM C che Pascal (atteso in `..\..\..\pasm.exe`) e confronta i binari.
- `test/test_simple.asm` — test minimale: LIA, LIB, STP, RTN
- `test/test_complicated.asm` — test esteso: opcode, .DB, label, salti, chiamate, espressioni, direttiva EQU
- `test/test_complicated_2.asm` — test esteso con funzionalità aggiuntive
- `test/test_complicated_3.asm` — test con molte istruzioni e casi particolari

Come funziona lo script di test
- Esegui `test/test_pasm.ps1` dalla subdirectory `test`, passando il nome del file `.asm` come parametro. Lo script:
  - compila con PASM C locale producendo `<nome>_c.bin` (opzionalmente con `-d` per generare `debug.txt`),
  - compila lo stesso `.asm` con PASM Pascal producendo `<nome>_pascal.bin`,
  - mostra l'hexdump e confronta i binari (`fc /b`).

Esempi di comandi (PowerShell)

```powershell
cd test
.\test_pasm.ps1 test_simple.asm
.\test_pasm.ps1 test_complicated.asm
.\test_pasm.ps1 test_complicated_2.asm
.\test_pasm.ps1 test_complicated_3.asm
```

## Stato dei test

**Test eseguiti e passati:**
- `test_simple.asm` — PASS
- `test_complicated.asm` — PASS
- `test_complicated_2.asm` — PASS

**Test falliti:**
- `test_complicated_3.asm` — FAIL (molte differenze tra binari C e Pascal)

**Note:**
- La versione C ora genera binari identici alla versione Pascal per i primi tre test, come verificato con `fc /b`.
- La gestione delle label e delle direttive (.DW, .DB, .ORG) è stata allineata e validata.
- **Da verificare ancora:**
  - La direttiva `.EQU` (definizione simboli)
- Per `test_complicated_3.asm` sono presenti molte differenze nei binari: analisi in corso.

---
Ultimo aggiornamento: 2025-11-25
