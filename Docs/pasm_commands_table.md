# Comandi Assembler - Riferimento

## Chiamate e Ritorni

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Range | Note |
|---------|--------|------------|--------|--------|------|-------|-------|------|
| CAL nm | 0xE0+n,m | PC+2→(R-1,R-2), R-2→R, nm→PC | - | - | 2 | 7 | 0≤n≤31 | → CALL HB:LB |
| CALL nm | 0x78,n,m | PC+3→(R-1,R-2), R-2→R, nm→PC | - | - | 3 | 8 | | → CALL HB:LB (scelta automatica) |
| RTN | 0x37 | (R-1,R-2)→PC, R+2→R | - | - | 1 | 4 | | |

## Addizione

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| ADB | 0x14 | (P)+A→(P), P+1→P, (P+1)+B+C→(P+1) | * | * | 1 | 5 | → ADDW [P+1]:[P], B:A |
| ADCM | 0xC4 | (P)+A+C→(P) | * | * | 1 | 3 | → ADDC [P], A |
| ADIA n | 0x74,n | A+n→A | * | * | 2 | 4 | |
| ADIM n | 0x70,n | (P)+n→(P) | * | * | 2 | 4 | |
| ADM | 0x44 | (P)+A→(P) | * | * | 1 | 3 | → ADD dst, src |
| ADN | 0x0C | (P)+A→(P)..., P-I-1→P, (P-I)+A→(P-I) BCD | * | * | 1 | 7+3*I | → ADDB [P], src |
| ADW | 0x0E | (P)+(Q)→(P)..., P-I-1→P, (P-I)+(Q-I)→(P-I), Q-I-2→Q BCD | * | * | 1 | 7+3*I | → ADDB [P], src |

## Sottrazione

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| SBB | 0x15 | (P)-A→(P), P+1→P, (P+1)-B-C→(P+1) | * | * | 1 | 5 | → SUBW [P+1]:[P], B:A |
| SBCM | 0xC5 | (P)-A-C→(P) | * | * | 1 | 3 | → SUBC [P], A |
| SBIA n | 0x75,n | A-n→A | * | * | 2 | 4 | |
| SBIM n | 0x71,n | (P)-n→(P) | * | * | 2 | 4 | |
| SBM | 0x45 | (P)-A→(P) | * | * | 1 | 3 | → SUB dst, src |
| SBN | 0x0D | (P)-A→(P), P-I-1→P, (P-I)-C→(P-I) BCD | * | * | 1 | 7+3*I | → SUBB [P], src |
| SBW | 0x0F | (P)-(Q)→(P), P-I-1→P, (P-I)-(Q-I)-C→(P-I), Q-I-2→Q BCD | * | * | 1 | 7+3*I | → SUBB [P], src |

## OR Logico

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| ORIA n | 0x65,n | A OR n→A | - | * | 2 | 4 | |
| ORID n | 0xD5,n | (DP) OR n→(DP) | - | * | 2 | 6 | |
| ORIM n | 0x61,n | (P) OR n→(P) | - | * | 2 | 4 | |
| ORMA | 0x47 | (P) OR A→(P) | - | * | 1 | 3 | → OR dst, src |

## AND Logico

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| ANIA n | 0x64,n | A AND n→A | - | * | 2 | 4 | |
| ANID n | 0xD4,n | (DP) AND n→(DP) | - | * | 2 | 6 | |
| ANIM n | 0x60,n | (P) AND n→(P) | - | * | 2 | 4 | |
| ANMA | 0x46 | (P) AND A→(P) | - | * | 1 | 3 | → AND dst, src |

## Shift di Bit

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| SL | 0x5A | C→A7...A0→C | - | - | 1 | 2 | → ROL |
| SLW | 0x1D | (P-1)...(P) 4 Bit SL, P-I-1→P | - | - | 1 | 5+I | → SLB [P] |
| SR | 0xD2 | C→A0...A7→C | * | - | 1 | 2 | → ROR |
| SRW | 0x1C | (P)...(P+I) 4 Bit SR, P+I+1→P | - | - | 1 | 5+I | → SRB [P] |

## Porte I/O

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| OUTA | 0x5D | (92)→IA-Port, 92→Q | - | - | 1 | 3 | → OUT A |
| OUTB | 0xDD | (93)→IB-Port, 93→Q | - | - | 1 | 2 | → OUT B |
| OUTC | 0xDF | (95)→C-PORT | - | - | 1 | 2 | → OUT C |
| OUTF | 0x5F | (94)→F0-Port, 94→Q | - | - | 1 | 3 | → OUT F |
| INA | 0x4C | IA-Port→A | - | * | 1 | 2 | → IN A |
| INB | 0xCC | IB-Port→A | - | * | 1 | 2 | → IN B |

## Contatori (Incremento)

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Q← | Note |
|---------|--------|------------|--------|--------|------|-------|-----|------|
| INCA | 0x42 | A+1→A | * | * | 1 | 4 | 2 | |
| INCB | 0xC2 | B+1→B | * | * | 1 | 4 | 3 | |
| INCI | 0x40 | I+1→I | * | * | 1 | 4 | 0 | |
| INCJ | 0xC0 | J+1→J | * | * | 1 | 4 | 1 | |
| INCK | 0x48 | K+1→K | * | * | 1 | 4 | 8 | |
| INCL | 0xC8 | L+1→L | * | * | 1 | 4 | 9 | |
| INCM | 0x4A | M+1→M | * | * | 1 | 4 | 10 | |
| INCN | 0xCA | N+1→N | * | * | 1 | 4 | 11 | |
| INCP | 0x50 | P+1→P | - | - | 1 | 2 | | |
| IX | 0x04 | X+1→X, X→DP | - | - | 1 | 6 | 5 | → INC X |
| IY | 0x06 | Y+1→Y, Y→DP | - | - | 1 | 6 | 7 | → INC Y |

## Contatori (Decremento)

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Q← | Note |
|---------|--------|------------|--------|--------|------|-------|-----|------|
| DECA | 0x43 | A-1→A | * | * | 1 | 4 | 2 | |
| DECB | 0xC3 | B-1→B | * | * | 1 | 4 | 3 | |
| DECI | 0x41 | I-1→I | * | * | 1 | 4 | 0 | |
| DECJ | 0xC1 | J-1→J | * | * | 1 | 4 | 1 | |
| DECK | 0x49 | K-1→K | * | * | 1 | 4 | 8 | |
| DECL | 0xC9 | L-1→L | * | * | 1 | 4 | 9 | |
| DECM | 0x4B | M-1→M | * | * | 1 | 4 | 10 | |
| DECN | 0xCB | N-1→N | * | * | 1 | 4 | 11 | |
| DECP | 0x51 | P-1→P | - | - | 1 | 2 | | |
| DX | 0x05 | X-1→X, X→DP | - | - | 1 | 6 | 5 | → DEC X |
| DY | 0x07 | Y-1→Y, Y→DP | - | - | 1 | 6 | 7 | → DEC Y |

## Salti Assoluti

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| JP nm | 0x79,n,m | nm→PC | - | - | 3 | 6 | → JMP HB:LB |
| JPC nm | 0x7F,n,m | IF C=1 nm→PC | - | - | 3 | 6 | → JPLO HB:LB |
| JPNC nm | 0x7D,n,m | IF C=0 nm→PC | - | - | 3 | 6 | → JPSH HB:LB |
| JPNZ nm | 0x7C,n,m | IF Z=0 nm→PC | - | - | 3 | 6 | → JPNE HB:LB |
| JPZ nm | 0x7E,n,m | IF Z=1 nm→PC | - | - | 3 | 6 | → JPEQ HB:LB |

## Salti Relativi

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| JRP n | 0x2C,n | PC+1+n→PC | - | - | 2 | 7 | → RJMP +dst |
| JRM n | 0x2D,n | PC-1-n→PC | - | - | 2 | 7 | → RJMP -dst |

## Salti Relativi Condizionali

| Comando | Opcode | Condizione | Operazione | Byte | Cicli | Note |
|---------|--------|------------|------------|------|-------|------|
| JRCM n | 0x3B,n | IF C=1 | PC+1-n→PC | 2 | 7/4 | → BRLO -dst |
| JRCP n | 0x3A,n | IF C=1 | PC+1+n→PC | 2 | 7/4 | → BRLO +dst |
| JRNCM n | 0x2B,n | IF C=0 | PC+1-n→PC | 2 | 7/4 | → BRSH -dst |
| JRNCP n | 0x2A,n | IF C=0 | PC+1+n→PC | 2 | 7/4 | → BRSH +dst |
| JRNZM n | 0x29,n | IF Z=0 | PC+1-n→PC | 2 | 7/4 | → BRNE -dst |
| JRNZP n | 0x28,n | IF Z=0 | PC+1+n→PC | 2 | 7/4 | → BRNE +dst |
| JRZM n | 0x39,n | IF Z=1 | PC+1-n→PC | 2 | 7/4 | → BREQ -dst |
| JRZP n | 0x38,n | IF Z=1 | PC+1+n→PC | 2 | 7/4 | → BREQ +dst |

## Movimenti di Blocchi

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| MVB | 0x0A | (Q)...(Q+J)→(P)...(P+J), P+J+1→P, Q+J+1→Q | - | - | 1 | 5+2*J | |
| MVBD | 0x1A | (DP)...(DP+J)→(P)...(P+J), P+J+1→P, DP+J→DP | - | - | 1 | 5+4*J | |
| MVW | 0x08 | (Q)...(Q+I)→(P)...(P+I), P+I+1→P, Q+I+1→Q | - | - | 1 | 5+2*I | |
| MVWD | 0x18 | (DP)...(DP+I)→(P)...(P+I), P+I+1→P, DP+I→DP | - | - | 1 | 5+4*I | |
| DATA | 0x35 | (BA)...(BA+I)→(P)...(P+I) | - | - | 1 | 11+4*I | Anche per ROM interna |
| FILD | 0x1F | A→(DP)...(DP+I), DP+I→DP | - | - | 1 | 4+3*I | |
| FILM | 0x1E | A→(P)...(P+I), P+I+1→P | - | - | 1 | 5+I | |

## Movimenti Semplici

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| MVMD | 0x55 | (DP)→(P) | - | - | 1 | 3 | |
| MVDM | 0x53 | (P)→(DP) | - | - | 1 | 3 | |
| LDD | 0x57 | (DP)→A | - | - | 1 | 3 | |
| STD | 0x52 | A→(DP) | - | - | 1 | 2 | |
| LDM | 0x59 | (P)→A | - | - | 1 | 2 | |
| LDP | 0x20 | P→A | - | - | 1 | 2 | |
| STP | 0x30 | A→P | - | - | 1 | 2 | |
| LDQ | 0x21 | Q→A | - | - | 1 | 2 | |
| STQ | 0x31 | A→Q | - | - | 1 | 2 | |
| LDR | 0x22 | R→A | - | - | 1 | 2 | |
| STR | 0x32 | A→R | - | - | 1 | 2 | |

## Caricamento Immediato

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Range | Note |
|---------|--------|------------|--------|--------|------|-------|-------|------|
| LIA n | 0x02,n | n→A | - | - | 2 | 4 | | |
| LIB n | 0x03,n | n→B | - | - | 2 | 4 | | |
| LII n | 0x00,n | n→I | - | - | 2 | 4 | | |
| LIJ n | 0x01,n | n→J | - | - | 2 | 4 | | |
| LIP n | 0x12,n | n→P | - | - | 2 | 4 | | |
| LIQ n | 0x13,n | n→Q | - | - | 2 | 4 | | |
| LP n | 0x80+n | n→P | - | - | 1 | 2 | 0≤n<63 | |
| LIDL m | 0x11,m | m→DPL | - | - | 2 | 5 | | |
| LIDP nm | 0x10,n,m | nm→DP | - | - | 3 | 8 | | |

## Movimenti con Auto-incremento/decremento

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Q← | Note |
|---------|--------|------------|--------|--------|------|-------|-----|------|
| IXL | 0x24 | X+1→X, X→DP, (DP)→A | - | - | 1 | 7 | 5 | → MOV A, [+X] |
| IYS | 0x26 | Y+1→Y, Y→DP, A→(DP) | - | - | 1 | 7 | 7 | → MOV [+Y], A |
| DXL | 0x25 | X-1→X, X→DP, (DP)→A | - | - | 1 | 7 | 5 | → MOV A, [-X] |
| DYS | 0x27 | Y-1→Y, Y→DP, A→(DP) | - | - | 1 | 7 | 7 | → MOV [-Y], A |

## Scambi

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| EXAB | 0xDA | A↔B | - | - | 1 | 3 | |
| EXAM | 0xDB | A↔(P) | - | - | 1 | 3 | |
| EXB | 0x0B | (P)...(P+J)↔(Q)...(Q+J), P+J+1→P, Q+J+1→Q | - | - | 1 | 6+3*J | |
| EXBD | 0x1B | (P)...(P+J)↔(DP)...(DP+J), P+J+1→P, DP+J→DP | - | - | 1 | 7+6*J | |
| EXW | 0x09 | (P)...(P+I)↔(Q)...(Q+I), P+I+1→P, Q+I+1→Q | - | - | 1 | 6+3*I | |
| EXWD | 0x19 | (P)...(P+I)↔(DP)...(DP+I), P+I+1→P, DP+I→DP | - | - | 1 | 7+6*I | |
| SWP | 0x58 | A0...A3↔A4...A7 | - | - | 1 | 2 | → SWAP |

## Test e Confronto

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| TEST n | 0x6B,n | TEST-Byte AND n | - | * | 2 | 4 | |
| TSIA n | 0x66,n | A AND n | - | * | 2 | 4 | |
| TSID n | 0xD6,n | (DP) AND n | - | * | 2 | 6 | |
| TSIM n | 0x62,n | (P) AND n | - | * | 2 | 4 | |
| CPIA n | 0x67,n | A-n | * | * | 2 | 4 | |
| CPIM n | 0x63,n | (P)-n | * | * | 2 | 4 | |
| CPMA | 0xC7 | (P)-A | * | * | 1 | 3 | |

## Gestione Flag

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| RC | 0xD1 | 0→C, 1→Z | * | * | 1 | 2 | Reset Carry |
| SC | 0xD0 | 1→C, 1→Z | * | * | 1 | 2 | Set Carry |

## Comandi Speciali

| Comando | Opcode | Operazione | Flag C | Flag Z | Byte | Cicli | Note |
|---------|--------|------------|--------|--------|------|-------|------|
| PTC | 0x7A | n→H, nm→(R-1,R-2), R-2→R | - | * | 4 | 9 | |
| ETC n,m,...,nm | 0x69 | FOR i=1 TO H: IF A=n nm→PC NEXT i | - | * | var | var | → CASE A |
| LOOP n | 0x2F,n | (R)-1→(R), IF C=0 PC+1-n→PC | * | * | 2 | 10/7 | |
| NOPT | 0xCE | No Operation | - | - | 1 | 3 | |
| NOPW | 0x4D | No Operation | - | - | 1 | 2 | → NOP |
| WAIT n | 0x4E,n | No Operation | - | - | 2 | 6+n | → NOP \<Number\> |

## Legenda

- **Flag C**: Carry flag (* = modificato, - = non modificato)
- **Flag Z**: Zero flag (* = modificato, - = non modificato)
- **Byte**: Numero di byte dell'istruzione
- **Cicli**: Numero di cicli macchina
- **→**: Indica la sintassi alternativa/mnemonico preferito

## Opcode Non Utilizzati

22, 23, 51, 54, 60, 61, 62, 63, 92, 94, 104, 106, 108-111, 114, 115, 118, 119, 123, 205, 207, 211, 215, 217, 220, 222

## SC-61860 CPU Register Set
     
The  SC61860  from  Sharp has 96 bytes of internal RAM which are used as registers and hardware stack.  The last  four  bytes  of
the  internal  RAM  are  special  purpose registers (I/O, timers, ...).  

Here is a list of the registers:  

        Reg     Address         Common use
        ---     -------         ----------
        I, J    0, 1            Length of block operations
        A, B    2, 3            Accumulator       
        XL, XH  4, 5            Pointer for read operations
        YL, YH  6, 7            Pointer for write operations
        K - N   8 - 0x0b        General purpose (counters ...)
          -     0x0c - 0x5b     Stack
        IA      0x5c            Input port A (IA-PORT)
        IB      0x5d            Input port B (IB-PORT)
        FO      0x5e            Output port F (F0-PORT)
        COUT    0x5f            Control port (C-PORT)


Other registers:

- 16 bit program counter (PC) 
- 16 bit data pointer (DP)
- 7 bit internal RAM pointers (P, Q), used in block operations
- 7 bit internal stack pointer (R)
- 8 bit data pointer (D), used in block operations

The ALU has a carry flag (C) and a zero flag (Z).   