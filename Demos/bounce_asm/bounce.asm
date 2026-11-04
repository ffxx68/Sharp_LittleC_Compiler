; Bounce - hand-written SC61860 assembly version of Demos\bounce\main.c (PC-1403)
; Same game logic: < > move the paddle, BRK exits.
;
; Build (from repository root):
;   pasm Demos\bounce_asm\bounce.asm bounce.bin
; Load at &E030, start with CALL &E030.

.ORG 0xE030

; game state in internal RAM (BASIC float-register area, scratch during CALL)
.equ KEYPREV 0x10   ; last key that moved the paddle (0xFF = none)
.equ CPOS    0x11   ; paddle x
.equ BX      0x12   ; ball x
.equ BY      0x13   ; ball y
.equ DXB     0x14   ; ball dx (+1 / -1)
.equ DYB     0x15   ; ball dy (+1 / -1)
.equ PNTX    0x16   ; score dot x
.equ PNTY    0x17   ; score dot y
.equ PNTM    0x18   ; score dot mode (1 set, 0 clear)
.equ LOOPN   0x19   ; ball delay down-counter
.equ SPOS    0x1A   ; snake ring-buffer index
.equ RND     0x1B   ; timer-based pseudo-random byte
.equ SNX     0x1C   ; snake x[4]
.equ SNY     0x20   ; snake y[4] (must be SNX+4)

START:
	LP	0		; save I..N for BASIC
	LIDP	SREG
	LII	11
	EXWD

	LP	KEYPREV		; load initial state
	LIDP	INITDATA
	LII	19
	MVWD

	LIA	1		; LCD on
	LIP	95
	EXAM
	OUTC

	LIA	0		; clear LCD
	LIDP	0x3000
	LII	59
	FILD
	LIDP	0x3040
	LII	59
	FILD

	CALL	DRAWPAD

MAIN_LOOP:
	TEST	2		; pseudo-random: count while 2ms timer bit is low
	JRNZP	RND_SKIP
	LP	RND
	ADIM	1
RND_SKIP:

	CALL	0x0E68		; ROM key scan -> A
	CPIA	0xFF
	JRNZP	KEY_DOWN
	LP	KEYPREV
	ORIM	0xFF
	JRP	BALL
KEY_DOWN:
	LP	KEYPREV
	CPMA
	JRZP	BALL		; same key still held
	CPIA	21		; > : right
	JRNZP	KEY_NOTR
	LP	CPOS
	CPIM	100
	JRNCP	BALL
	CALL	ERASEPAD
	LP	CPOS
	ADIM	5
	JRP	KEY_DRAW
KEY_NOTR:
	CPIA	16		; < : left
	JRNZP	BALL
	LP	CPOS
	CPIM	0
	JRZP	BALL
	CALL	ERASEPAD
	LP	CPOS
	SBIM	5
KEY_DRAW:
	CALL	DRAWPAD

BALL:
	LP	LOOPN
	SBIM	1
	JPNZ	LOOP_END
	LIA	10
	EXAM

	LP	8		; erase snake tail
	ANIM	0
	CALL	SNAKEPTR	; P -> SNX[SPOS]
	LDM
	EXAB
	LDP
	ADIA	4
	STP
	LDM
	EXAB			; A = tail x, B = tail y
	CALL	PLOT

	LP	BX		; x bounds
	LDM
	LP	DXB
	CPIA	0
	JRNZP	X_NOT0
	LIA	1
	EXAM
	JRP	X_DONE
X_NOT0:
	CPIA	99
	JRNZP	X_DONE
	LIA	0xFF
	EXAM
X_DONE:

	LP	BY		; y bounds
	LDM
	LP	DYB
	CPIA	6
	JRNZP	Y_NOT6
	LIA	0xFF
	EXAM
	JRP	MOVE
Y_NOT6:
	CPIA	1
	JRNZP	MOVE
	LIA	1
	EXAM

	LP	RND		; random x inversion on bottom bounce
	LDM
	CPIA	129
	JRCP	NO_INV
	LP	DXB
	LDM
	ANIM	0
	SBM			; dx = -dx
NO_INV:

	LP	BX		; paddle hit: (x - cpos) < 5
	LDM
	EXAB
	LP	CPOS
	LDM
	LP	3
	SBM
	LDM
	CPIA	5
	JRNCP	MOVE

	LP	PNTM		; score: plot dot, advance
	LDM
	LP	8
	EXAM
	LP	PNTY
	LDM
	EXAB
	LP	PNTX
	LDM
	CALL	PLOT
	LP	PNTX
	ADIM	1
	CPIM	120
	JRCP	MOVE
	LIA	100
	EXAM
	LP	PNTY
	SBIM	1
	JRNZP	MOVE
	LIA	6
	EXAM
	LP	PNTM
	SBIM	1		; toggle 1 <-> 0
	ANIM	1

MOVE:
	LP	DXB
	LDM
	LP	BX
	ADM
	LP	DYB
	LDM
	LP	BY
	ADM

	CALL	SNAKEPTR	; store new head in ring buffer
	LDP
	EXAB			; B = &SNX[SPOS]
	LP	BX
	LDM
	EXAB
	STP
	EXAB
	EXAM			; SNX[SPOS] = x
	LDP
	ADIA	4
	EXAB			; B = &SNY[SPOS]
	LP	BY
	LDM
	EXAB
	STP
	EXAB
	EXAM			; SNY[SPOS] = y

	LP	SPOS
	ADIM	1
	ANIM	3

	LIA	1		; draw head
	LP	8
	EXAM
	LP	BY
	LDM
	EXAB
	LP	BX
	LDM
	CALL	PLOT

LOOP_END:
	TEST	8		; BRK
	JPZ	MAIN_LOOP

	LP	0		; restore I..N, back to BASIC
	LIDP	SREG
	LII	11
	EXWD
	RTN

; P -> SNX[SPOS]
SNAKEPTR:
	LP	SPOS
	LDM
	ADIA	SNX
	STP
	RTN

; paddle: draw (K=1) / erase (K=0) 5 dots at (CPOS, 0)
DRAWPAD:
	LIA	1
	LP	8
	EXAM
	JRP	PAD
ERASEPAD:
	LP	KEYPREV		; remember key that moved the paddle (A = key)
	EXAM
	LP	8
	ANIM	0
PAD:
	LP	CPOS
	LDM
	LP	9
	EXAM			; L = x
	LIA	5
	LP	10
	EXAM			; M = count
PAD_LOOP:
	LP	9
	LDM
	LIB	0
	CALL	PLOT
	INCL
	DECM
	JRNZM	PAD_LOOP
	RTN

; set (K=1) / clear (K=0) pixel A = x (0..119, else ignored), B = y (0 = bottom .. 6)
; uses A, B, X, Yl
PLOT:
	CPIA	120
	JRNCP	PL_RET
	LP	4
	EXAM			; Xl = x
	LIA	0x40		; mask = 0x40 >> y
PL_SH:
	DECB
	JRCP	PL_MSK
	SR
	JRM	PL_SH
PL_MSK:
	LP	6
	EXAM			; Yl = mask
	LP	4
	LDM			; A = x -> LCD address low byte
	CPIA	30
	JRCP	PL_MAP		; 0..29   -> x
	CPIA	45
	JRNCP	PL_M1
	ADIA	15		; 30..44  -> x+15
	JRP	PL_MAP
PL_M1:
	CPIA	60
	JRNCP	PL_M2
	SBIA	15		; 45..59  -> x-15
	JRP	PL_MAP
PL_M2:
	LIB	168		; 60..74  -> 168-x
	CPIA	75
	JRCP	PL_NEG
	LIB	198		; 75..89  -> 198-x
	CPIA	90
	JRCP	PL_NEG
	LIB	183		; 90..119 -> 183-x
PL_NEG:
	LP	3
	SBM
	EXAB
PL_MAP:
	LP	4
	EXAM
	LIA	0x30
	LP	5
	EXAM			; X = 0x3000 + addr
	DX
	IX			; DP = X
	LP	8
	CPIM	0
	LP	6
	LDD
	JRZP	PL_CLR
	ORMA
	JRP	PL_WR
PL_CLR:
	ANMA
	EXAM
	SBM			; byte & ~mask
PL_WR:
	LDM
	STD
PL_RET:
	RTN

SREG:
	.DW	0, 0, 0, 0, 0, 0

INITDATA:
	.DB	0xFF		; KEYPREV
	.DB	50		; CPOS
	.DB	50, 6		; BX, BY
	.DB	1, 0xFF		; DXB, DYB
	.DB	100, 6, 1	; PNTX, PNTY, PNTM
	.DB	11		; LOOPN
	.DB	0, 0		; SPOS, RND
	.DB	0, 0, 0, 0	; SNX
	.DB	0, 0, 0, 0	; SNY
