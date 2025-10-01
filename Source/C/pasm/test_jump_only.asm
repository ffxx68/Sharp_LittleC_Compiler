; Test isolato salto relativo
.ORG 40000
.EQU regB 3

.DB 0x10, 0x20, 0x30

START:
    LIA regB
    JRP NEXT

NEXT:
    NOP
