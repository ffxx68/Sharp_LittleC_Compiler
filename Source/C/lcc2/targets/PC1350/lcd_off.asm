; Clear LCD enable bit without changing the other control-port bits.
lcd_off:
	LIDP	PC1350_LCD_SAVED_IRAM
	LP	8
	LII	87
	EXWD
	LIDP	PC1350_LCD_SAVED_IRAM+87
	LDD
	ANIA	0xFE
	LIP	95
	EXAM
	OUTC
	LIDP	PC1350_LCD_SAVED_IRAM
	LP	8
	LII	87
	MVWD
	RTN
