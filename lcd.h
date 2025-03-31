
lcd_dummy ( ) {
#asm
.include _lcd_pc1403_pset.lib
.include _lcd_pc1403_cls.lib
#endasm
}

// set pixel
lcd_pset(byte bLcdXpos1, byte bLcdYpos1, byte bLcdMode1) 
{
	regX = bLcdXpos1; // 0 = left
	regY = bLcdYpos1; // 0 = top
	regK = bLcdMode1; // 1 = set, 0 = clear, 2 = invert
#asm
	call LCD_LIB_PSET
#endasm
	return;
}

// clear screen 
lcd_cls(byte bClsMode) 
{
	regK = bClsMode; // 1 = fill, 0 = clear, 
#asm
	call LCD_LIB_CLS
#endasm
	return;
}

// draws a horizontal line starting at bLcdXpos2, bLcdYpos2 of length bLen2
lcd_hline(byte bLcdXpos2, byte bLcdYpos2, byte bLen2, byte bLcdMode2)
{	
#asm
	LDR
	ADIA 3 ; local bLen2 to A
	STP
	LDM
	LP	1	; Store to J
	EXAM
	LDR
	ADIA 5 ; local bLcdXpos2 into A
	STP
	LDM
	LP  1
	ADM 	; A + J -> J  (running position, starting from end: bLcdXpos2+bLen2 )
	
lcd_hline_loop0:
	DECJ    ; e.g. bLen2 = 1 means one dot at position bLcdXpos2+0
	LP  1
	LDM     ; J -> A
	LP	4	; Store A to X-low
	EXAM

	LDR
	ADIA 4 ; local bLcdYpos2 to A
	STP
	LDM
	LP	6	; store to Y-low
	EXAM	
	
	LDR
	ADIA 2 ; local bLcdMode2 to A
	STP
	LDM
	LP	8	; Store to K
	EXAM	

	; takes, X, Y and K as parameters
	; note - J not used inside!
	call LCD_LIB_PSET
	
	LDR
	ADIA 5 ; local bLcdXpos2 into A (end position)
	STP
	LDM
	
	LP	1 ; compare J with A
	CPMA  
	JRZP lcd_hline_end  ; 
	JRM lcd_hline_loop0 ; continue until J<A 
lcd_hline_end:
	RTN
#endasm
}