#ifndef LCC2_LCD_H
#define LCC2_LCD_H

/* PC-1350, PC-1401 and PC-1403 LCD controls; not standard C stream operations. */
void lcd_on(void);
void lcd_off(void);
/* PC-1403 only: mode 0 clears pixels; 1 sets pixels. */
void lcd_cls(unsigned char mode);
/* PC-1350 lcd_pset: x=0..149, y=0..31 (0 is the bottom row).
   PC-1401/PC-1403 lcd_pset: x=0..79/119, y=0..6 (0 is the bottom row).
   lcd_pset mode 0 clears; 1 sets. Invalid coordinates or modes do nothing. */
void lcd_pset(unsigned char x, unsigned char y, unsigned char mode);
/* PC-1403 only: draw length pixels from x at y, clipping at the right edge. */
void lcd_hline(unsigned char x, unsigned char y, unsigned char length,
               unsigned char mode);

#endif
