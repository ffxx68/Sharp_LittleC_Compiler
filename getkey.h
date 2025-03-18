char getkey () {
#asm	
    CALL  0x1494        ; Syscall: wait for a keystroke (from PC-1403 ROM)
	LIA	LB(0xFF5E)	    ; LB of key-code address
	LIB	HB(0xFF5E)	    ; HB of key-code address
	LP 4
	EXAM		        ; LB -> Xl
	EXAB
	INCP		
	EXAM                ; HB -> Xh
	DX
	IXL                 ; set return value in A
#endasm
	return;
}
