# Refactor roadmap per `lcc` (SC61860)

Questo documento raccoglie l'analisi e il piano di refactor per rendere più modulare, leggibile e manutenibile il compilatore `lcc` presente in `Source/lcc`.

## Sommario esecutivo
- Obiettivo: separare responsabilità (lexing, parsing, symbol table, semantic, codegen, output, ottimizzazioni, utilità) in unit Pascal distinte, mantenendo retrocompatibilità e permettendo refactor incrementali e verificabili.
- Approccio: refactor incrementale in fasi, test e compilazione dopo ogni fase, wrapper compatibili per preservare API pubbliche.

---

## Checklist iniziale (da completare)
- [ ] Creare branch git `refactor/modular-lcc` (consigliato).
- [ ] Eseguire build baseline (compilare `Source/lcc`) e salvare output/log.
- [ ] Creare file di roadmap (questo file).

---

## Mappa componenti attuali (high-level)
- `input.pas`: lettura carattere, `GetChar`, variabile globale `Look`.
- `scanner.pas`: tokenizzazione (GetToken, GetName, GetNumber, GetFloat), utilità di parsing stringhe, modalità di input (file/string/keyboard), variabili globali `Tok`, `dummy`, `Level`, `md`, `linecnt`.
- `parser.pas` / `parser_new.pas`: parsing, symbol table (VarList/ProcList), semantic actions (AddVar/AddProc/AllocVar/repadr), generazione asm diretta, ottimizzazioni post-process.
- `CodeGen.pas`: generazione label, helper varxram/varcode/varreg, op helpers (CompEqual, CompGreater, ...), gestione librerie.
- `output.pas`: accumulo `asmtext`/`libtext`/`asmlist`, funzioni `writeln` wrapper e `addasm`.
- `errors.pas`: Error/Expected che usano variabili globali del scanner.
- `calcunit.pas`: Evaluate() per valutazione espressioni matematiche.

---

## Problemi principali identificati
- Parsing, semantic analysis e emissione di codice mescolati nello stesso file (`parser.pas`).
- Forti globali condivise tra unit (Look, Tok, dummy, linecnt, pushcnt, optype...).
- Duplicazione tra `parser.pas` e `parser_new.pas`.
- Post-processing/ottimizzazioni mescolati nel `SecondScan`.

---

## Componenti target (proposti)
Per ogni componente riporto responsabilità, API minime, dipendenze e priorità.

1) `Lexer` (estratto da `scanner.pas`)
- Responsabilità: lexing/tokenization, utilità stringhe strettamente necessarie al lexer.
- API minima: `InitFromFile`, `InitFromString`, `GetToken(mode, var s)`, `GetName`, `GetNumber`, `GetFloat`, `CopyToken`, `CurrentToken`, `CurrentLine`.
- Dipendenze: `Input`, `Errors`.
- Priorità: Alta.

2) `SymbolTable` (estratto da parser VarList/ProcList)
- Responsabilità: gestione VarList/ProcList, Find/Add/Alloc/IsVarAtAdr/RemoveLocalVars.
- API minima: `FindVar`, `AddVar`, `AllocVar`, `FindProc`, `AddProc`, `RemoveLocalVars`, getters per metadati.
- Dipendenze: `CalcUnit` (per mathparse), `Errors`.
- Priorità: Alta.

3) `Semantic` (logica vardecl / repadr / controlli)
- Responsabilità: vardecl parsing helper, repadr, validazioni di tipo/overlap.
- API minima: `ParseVarDecl(tok: string): string` (o wrapper), `RepAdr(currproc)`.
- Dipendenze: `SymbolTable`, `CalcUnit`, `Errors`.
- Priorità: Alta.

4) `CodeGen` (rifattorizzazione)
- Responsabilità: emissione istruzioni, gestione labels, gestione librerie, helper varXxx.
- API minima: `NewLabel`, `PostLabel`, `EmitInst`, `AddLib`, `VarXram`, `VarCode`, `VarReg`, `Flush`.
- Dipendenze: `Output`.
- Priorità: Alta.

5) `Output` (pulito)
- Responsabilità: accumulo di `asmtext`/`libtext`/`asmlist`, scrittura su file.
- API minima: `Emit`, `AddAsm`, `SaveToFile`, `Reset`.
- Priorità: Alta.

6) `Backend` (ottimizzazioni e post-processing)
- Responsabilità: logiche di trasformazione `asmtext` (pattern replace), compressione istruzioni, step attualmente in `SecondScan`.
- API minima: `OptimizeAsm(asmText): string`, `WriteAsmFile(filename, asmText, libText)`.
- Priorità: Media-Alta.

7) `CalcUnit` (ripulire)
- Responsabilità: Evaluate() per espressioni e conversioni hex/bin.
- API minima: `Evaluate`, `ConvertHex`, `ConvertBin`.
- Priorità: Media.

8) `Errors` (migliorare)
- Responsabilità: reporting errori con contesto (line, token, dummy).
- API minima: `Error(msg)`, `Expected(msg)`, `Warning(msg)`.
- Priorità: Media.

---

## Task concreti e ordinati (iterazioni incrementali)
Ogni item è pensato per essere piccolo e verificabile.

Fase 0 — Preparazione (0.5 gg)
- Task 0.1: creare branch git per il refactor.
- Task 0.2: run build baseline (compilare `Source/lcc`) e salvare output.

Fase 1 — Estrarre `Lexer` (2 gg)
- Task 1.1: creare `Lexer.pas` che re-esporta le funzioni pubbliche di `scanner.pas` (GetToken, GetName...).
- Task 1.2: sostituire usi diretti in `parser` con `Lexer` (usare wrappers per mantenere compatibilità).
- Verifica: build OK, test su file demo (token stream atteso).

Fase 2 — Estrarre `SymbolTable` e `Semantic` (3 gg)
- Task 2.1: creare `SymbolTable.pas` e spostare VarList/ProcList, Find/Add/Alloc.
- Task 2.2: creare `Semantic.pas` per vardecl/repadr/controlli.
- Verifica: FirstScan produce tavole var/proc identiche al baseline.

Fase 3 — Refactor `CodeGen` + `Output` (3 gg)
- Task 3.1: aggiungere `EmitInst` in CodeGen e usare `Output.Emit` al posto di `writeln` diretto.
- Task 3.2: consolidare `addlib` e gestione `libtext` in `CodeGen` -> `Output`.
- Verifica: SecondScan genera asm funzionante; build OK.

Fase 4 — Ridurre Parser: separare sintassi da emissione (4-6 gg)
- Task 4.1: sostituire generazione asm inline con chiamate a `CodeGen.Emit*`.
- Task 4.2: valutare introduzione AST (opzionale) per espressioni complesse.
- Verifica: output asm semantico identico; test su demo.

Fase 5 — Backend e ottimizzazioni (2-3 gg)
- Task 5.1: estrarre logiche di ottimizzazione (temp -> temp2 passes) in `Backend.pas`.
- Task 5.2: creare test per le ottimizzazioni (input con pattern noto => output atteso).
- Verifica: ottimizzazioni mantengono equivalenza e non introducono regressioni.

Fase 6 — Hardening, cleanup, docs e test (2 gg)
- Task 6.1: rimuovere duplicati (`parser` vs `parser_new`) dopo verifica e consolidamento.
- Task 6.2: aggiornare README, aggiungere piccoli script/ci per test automatici.
- Verifica: build + smoke tests verdi.

Stima totale: ~14-18 giornate.

---

## Rischi principali e mitigazioni
- R: rottura API perché molte funzioni fanno uso di variabili globali: mitigare creando wrappers identici e spostando progressivamente.
- R: differenze tra `parser.pas` e `parser_new.pas`: mitigare comparando outputs su suite di test e scegliere la base migliore.
- R: ottimizzazioni basate su matching testuali molto sensibili a formato/whitespace: mitigare creando funzioni di trasformazione documentate e testate.

---

## Suggerimenti pratici per lo sviluppo
- Fare commit frequenti e piccoli per ogni estrazione ("extract Lexer", "extract SymbolTable").
- Aggiungere un piccolo harness che lanci `lcc` su un elenco di demo e confronti l'asm prodotto con file di riferimento (regressione).
- Usare wrapper per preservare le chiamate pubbliche esistenti (es. `GetToken`) e poi deprecare direttamente i vecchi file.

---

## Checklist di qualità (Quality Gates)
- Build: compila l'intero `Source/lcc` dopo ogni fase — PASS.
- Lint/Typecheck: verificare warning e risolverli — preferibile PASS con zero warning critici.
- Unit tests: aggiungere test per `Lexer`, `SymbolTable`, `Backend.OptimizeAsm` — PASS.
- Smoke tests: lanciare `lcc` su 3-5 demo e verificare che l'asm sia assemblabile — PASS.

---

## Criteri di accettazione
- Tutte le unit del nuovo set compilano senza errori con FPC/Delphi.
- Il comportamento esterno di `lcc` sui demo di riferimento rimane equivalente (output assemblatore assemblabile con `pasm`).
- Il `parser` non emette più stringhe asm direttamente ma chiama `CodeGen`/`Output` (obiettivo intermedio verificabile).
- Documentazione aggiornata (`Source/lcc/README.md` o questo `refactor_roadmap.md`).

---

## Esempi veloci di stub di unità (da implementare come primo passo)

`Lexer.pas` (interfaccia suggerita):
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
// ...existing code... (migrato da scanner.pas) ...
end.
```

`SymbolTable.pas` (interfaccia suggerita):
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
// ...existing code... (migrato da parser.pas VarList/ProcList) ...
end.
```

---

## Prossimi passi raccomandati (subito)
1. Confermi che posso creare il branch e iniziare implementando `Lexer.pas` come wrapper di `scanner.pas`? (posso eseguire e compilare in seguito). 
2. Se preferisci, posso invece generare subito gli stub file (`Lexer.pas` e `SymbolTable.pas`) e testarli con una compilazione minima.

---

File creato automaticamente: `Source/lcc/refactor_roadmap.md`.

