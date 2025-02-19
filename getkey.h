// using routines from PC-1403 ROM
char getkey () {
#asm	
    CALL  0x1494        ; Syscall: wait for a keystroke (PC-1403)
#endasm
	return readbyte(0xFF5E); // get the key-code value stored in memory
}
