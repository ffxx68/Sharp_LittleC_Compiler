# PASM Migration from Pascal to C

## Project Overview

Questo progetto documenta la migrazione dell'assembler **PASM** dall'originale Pascal a C, con l'obiettivo di mantenere piena compatibilità funzionale e output binario identico.

**Ultima modifica:** 20 novembre 2025

## Stato attuale - 🔄 VALIDAZIONE IN CORSO

### ⚠️ Test di validazione

- La migrazione del codice è completa e i test iniziali sono in corso
- **Test 2 (test_jump_only.asm)** ha prodotto output binario identico
- **Test 1 (test_jrm.asm)** presenta differenze tra i file binari generati
- Sono richiesti ulteriori test su funzionalità avanzate e casi limite
- L'assembler NON è ancora considerato pronto per la produzione finché tutti i test non saranno superati

## Script di confronto

Per automatizzare la verifica tra i file binari generati dalle versioni C e Pascal di pasm.exe, è disponibile lo script PowerShell `test_pasm.ps1` nella cartella `Source/C/pasm`. Lo script:
- Compila il file .asm indicato con entrambe le versioni di pasm
- Genera i file binari con suffisso `_c.bin` e `_pascal.bin`
- Esegue automaticamente il confronto binario tramite `fc.exe /b`
- Mostra a terminale eventuali differenze

Esempio d'uso:
```
.\test_pasm.ps1 nomefile.asm
```

## Verifica compatibilità - ⚠️ SUCCESSO PARZIALE

### Test eseguiti

#### Test 1: Direttive base e salti all'indietro ❌ FALLITO
- **File di test:** `test_jrm.asm` (ex test_org_equ.asm)
- **Confronto binario:** `fc.exe /b test_jrm_c.bin test_jrm_pascal.bin`
- **Risultato:** **DIFFERENZE TROVATE** ❌
- **Note:** I file binari generati non sono identici. Serve analisi e correzione della logica di generazione.

#### Test 2: Salto relativo in avanti e risoluzione etichette ✅ SUPERATO
- **File di test:** `test_jump_only.asm`
- **Confronto binario:** `fc.exe /b test_jump_only_c.bin test_jump_only_pascal.bin`
- **Risultato:** **IDENTICI byte per byte** ✅
- **Debug output:** File `debug.txt` generato correttamente con traccia di risoluzione etichette

#### Test 3: Funzionalità estese ⏳ IN ATTESA
- **File di test:** `test_extended_features.asm`
- **Stato:** **NON ANCORA TESTATO**
- **Da verificare:** Costrutti complessi, casi limite, direttive avanzate

### Funzionalità verificate
- ✅ Gestione di `.ORG 40000` e `.EQU regB 3`
- ✅ Assembly di `LIA regB` (02 03)
- ✅ Salti relativi in avanti `JRP NEXT`
- ✅ Risoluzione etichette forward
- ✅ Generazione file binario identico a Pascal (test 2)
- ✅ Modalità debug (-d flag)

### Funzionalità da verificare
- ⏳ Funzionalità estese in `test_extended_features.asm`
- ⏳ Casi limite e condizioni di errore
- ⏳ Compatibilità con tutti i file demo

## Struttura file

```
Source/C/pasm/
├── pasm.c                      # Main assembler C file
├── pasm_constants.h            # Header with protected OPCODE/NBARGU arrays
├── pasm.exe                    # Generated C executable
├── debug.txt                   # Debug output (when -d flag used)
├── test_jrm.asm                # Assembly test file (backward jumps, ex test_org_equ.asm)
├── test_jrm_pascal.bin         # Pascal reference output
├── test_jrm_corrected.bin      # Verified identical C output
├── test_jump_forward.asm       # Assembly test file (forward jumps)
├── test_jump_forward_c.bin     # C version output
├── test_jump_forward_pascal.bin # Pascal reference output
├── test_jump_only.asm          # Assembly test file (relative jump only)
├── test_jump_only_c.bin        # C version output
├── test_jump_only_pascal.bin   # Pascal reference output
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

## Prossimi passi - 🔄 TEST IN ATTESA

### Task da completare
- ⏳ **Analisi e correzione della logica di generazione per test 1**
- ⏳ **Test funzionalità estese** con `test_extended_features.asm`
- ⏳ **Verifica costrutti complessi**
- ⏳ **Test casi limite e gestione errori**
- ⏳ **Validazione di tutti i file demo**

### Miglioramenti futuri opzionali
- [ ] Performance benchmarking vs Pascal version
- [ ] Extended error reporting and validation
- [ ] Integration with build system

## Sommario stato attuale

**La migrazione PASM C è FUNZIONALMENTE COMPLETA ma richiede ulteriore validazione.** 

✅ **Funzionalità verificate:**
- Salti relativi in avanti (JRP)
- Risoluzione etichette forward
- Direttive base (.org, .equ)
- Modalità debug

❌ **Non ancora compatibile:**
- Salti all'indietro (JRM) e direttive base (test_jrm.asm)

⏳ **Da testare:**
- Funzionalità estese e costrutti complessi
- Casi limite e condizioni di errore
- Compatibilità demo completa

---
*Stato al: 20 novembre 2025 - Validazione parziale, test estesi in attesa*
