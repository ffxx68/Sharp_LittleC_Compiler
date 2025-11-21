# PASM (Assembler) — Tests and Verification

Questa cartella contiene la versione C del PASM assembler e una suite di test per la verifica binaria rispetto all'implementazione Pascal originale.

File di interesse
- `pasm.c`, `pasm.exe` (build locale)
- `test/test_pasm.ps1` — script PowerShell che esegue sia il PASM C che quello Pascal (atteso in `..\..\..\pasm.exe`) e confronta i binari.
- `test/test_simple.asm` — test minimale: LIA, LIB, STP, RTN
- `test/test_complicated.asm` — test esteso: opcode, .DB, label, salti, chiamate, espressioni

Come funziona lo script di test
- Esegui `test/test_pasm.ps1` dalla sottodirectory `test` passando il nome del file `.asm` come parametro. Lo script:
  - compila con il PASM C locale producendo `<name>_c.bin` (opzionalmente con `-d` per generare `debug.txt`),
  - compila lo stesso `.asm` con il PASM Pascal producendo `<name>_pascal.bin`,
  - mostra gli hexdump e confronta i binari (`fc /b`).

Esempi di comandi (PowerShell)

```powershell
cd test
.\test_pasm.ps1 test_simple.asm
.\test_pasm.ps1 test_complicated.asm
```

## Test status

**Test eseguiti e passati:**
- `test_simple.asm` — PASS/FAIL (aggiorna dopo verifica)
- `test_complicated.asm` — PASS/FAIL (aggiorna dopo verifica)

Aggiorna questa sezione dopo ogni ciclo di verifica per tenere traccia dello stato dei test.
