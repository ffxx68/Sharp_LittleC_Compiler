// set pixel
lcd_pset(byte bLcdXpos, byte bLcdYpos, byte bLcdMode) 
{
	regX = bLcdXpos; // 0 = left
	regY = bLcdYpos; // 0 = top
	regK = bLcdMode; // 1 = set, 0 = clear, 2 = invert
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


