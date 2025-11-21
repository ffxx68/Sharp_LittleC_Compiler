; Test complicato PASM
.ORG 40000
.include "test_include.inc"
START:      LIA 0x10
            LIB 0x20
            STP 0x30
            LIA 2
            LIB 3
            .DB 0x2A, 0x4D, 0x2C, 0x0D, 0x2D, 0x08, 0xFF, 0xFF
            .DB 1, 2, 3, 4, 0xAA, 0xBB
            RTN
; Test di salti e chiamate
            CALL START
            JP START
            JRM START
            JRP START
; Test con espressioni
VAL1:       EQU 1+2+3
VAL2:       EQU (2*2)+1
            LIA VAL1
            LIA VAL2
            .DB VAL1, VAL2
