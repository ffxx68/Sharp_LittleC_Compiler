// set pixel
lcd_pset(byte bLcdXpos1, byte bLcdYpos1, byte bLcdMode1) 
{
	regX = bLcdXpos1; // 0 = left
	regY = bLcdYpos1; // 0 = top
	regK = bLcdMode1; // 1 = set, 0 = clear, 2 = invert
#asm
	call LCD_PC1403LIB_PSET
#endasm
	return;

#asm
.include _lcd_pset.lib
#endasm
}

// clear screen 
lcd_cls(byte bClsMode) 
{
	regK = bClsMode; // 1 = fill, 0 = clear, 
#asm
	call LCD_PC1403LIB_CLS
#endasm
	return;

#asm
.include _lcd_cls.lib
#endasm
}

// draws a horizontal line starting at bLcdXpos2, bLcdYpos2 of length bLen2
lcd_hline(byte bLcdXpos2, byte bLcdYpos2, byte bLen2, byte bLcdMode2)
{
	byte b_lcd_hline_Idx1;
	for (b_lcd_hline_Idx1=0; b_lcd_hline_Idx1<=bLen2; b_lcd_hline_Idx1++) {
		lcd_pset (bLcdXpos2+b_lcd_hline_Idx1, bLcdYpos2, bLcdMode2) ;
	}
	return;
}