; pasm file - assemble with pasm!
; Compiled with lcc v1.1

.ORG	33000

	LP	0
	LIDP	SREG
	LII	11
	EXWD

	CALL	main

	LP	0
	LIDP	SREG
	LII	11
	MVWD
	RTN

SREG:	.DW 0, 0, 0, 0, 0, 0


main:	; Procedure
	; If block: Boolean expression

	LIDP	32995	; Load variable a
	LDD
	PUSH ; byte
	LIDP	32999	; Load variable c
	LDD
	EXAB
	POP
	LP	3
	CPMA		; Compare for equal
	LIA	0
	JRNZP	2
	DECA

	TSIA	255	; Branch if false
	JPZ	LB0


	; If expression = true
	LIA	1	; Load byte constant 1
	LIDP	32997	; Store result in b
	STD

	JP	LB1
  LB0:
	; If expression = false
	LIA	0	; Load byte constant 0
	LIDP	32997	; Store result in b
	STD

	; End of if
  LB1:

	; If block: Boolean expression

	LIDP	32995	; Load variable a
	LDD
	LIB	100	; Load byte constant 100
	EXAB
	LP	3
	CPMA		; Compare for Greater or Equal
	LIA	0
	JRCP	2
	DECA

	PUSH ; byte
	LIDP	32997	; Load variable b
	LDD
	LIB	10	; Load byte constant 10
	EXAB
	LP	3
	CPMA		; Compare for smaller
	LIA	0
	JRNCP	2
	DECA

	EXAB
	POP
	LP	3
	ANMA		; AND
	EXAB
	TSIA	255	; Branch if false
	JPZ	LB2

	; If expression = true
	LIA	5	; Load byte constant 5
	LIDP	32999	; Store result in c
	STD

	; a++
	LIDP	32995	; Load variable a
	LDD
	INCA
	LIDP	32995	; Store result in a
	STD

	; End of if
  LB2:

	RTN	; end of main



