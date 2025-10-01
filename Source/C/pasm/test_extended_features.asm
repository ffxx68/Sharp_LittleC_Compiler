; Test esteso PASM - direttive, etichette, salti, include, commenti
; Verifica compatibilità e parsing

.ORG 40000         ; Imposta indirizzo di partenza
.EQU regB 3        ; Definisce simbolo regB
.EQU valC 42       ; Simbolo per valore costante

.INCLUDE test_include.inc ; Include esterno senza virgolette, compatibile con PASM Pascal

START:             ; Etichetta di inizio
    LIA regB       ; Istruzione con simbolo .EQU
    LIA valC       ; Istruzione con simbolo .EQU diverso
    ADD regB, valC ; Istruzione con 2 argomenti
    MOV regB, valC, 7 ; Istruzione con 3 argomenti
    NOP            ; Istruzione senza argomenti
    JRP NEXT        ; Salto relativo avanti (JRP supportato da PASM Pascal)
    JRM START      ; Salto relativo indietro
    ; Commento su riga singola
    LIA 0xFF       ; Valore esadecimale
    LIA 255        ; Valore decimale
    .DB 1,2,3,4    ; Definizione dati
    .DB 0xAA, 0xBB ; Dati esadecimali

NEXT:              ; Seconda etichetta
    NOP            ; Istruzione semplice
    ; Commento inline dopo istruzione
    LIA 1 ; Inline comment

    ; Test whitespace e case-insensitive
    lia    2
    LiA    3
    lIa    4

    ; Test direttiva .ORG multipla
    .ORG 40100
    LIA 5

; Fine file
