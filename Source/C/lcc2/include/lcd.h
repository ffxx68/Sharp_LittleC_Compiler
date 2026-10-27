#ifndef LCC2_LCD_H
#define LCC2_LCD_H

/* PC-1403 LCD controls; not standard C stream operations. */
void lcd_on(void);
void lcd_off(void);
/* mode: 0 clears pixels; 1 sets pixels. Other values do nothing. */
void lcd_cls(unsigned char mode);
/* Coordinates: x=0..119, y=0..6 (0 is the bottom row).
   mode: 0 clears; 1 sets. Invalid coordinates or modes do nothing. */
void lcd_pset(unsigned char x, unsigned char y, unsigned char mode);
/* Draw length pixels from x at y, clipping at the right edge. */
void lcd_hline(unsigned char x, unsigned char y, unsigned char length,
               unsigned char mode);

#endif
