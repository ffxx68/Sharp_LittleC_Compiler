#ifndef LCC2_KEY_H
#define LCC2_KEY_H

/* Target-specific raw key codes, not ASCII (PC-1401 and PC-1403). */
unsigned char key_wait(void); /* Blocks until a key is pressed. */
/* Nonblocking; returns 0xFF when no key is pressed.
   PC-1401 codes: 0..76; PC-1403 codes: 0..83. */
unsigned char key_scan(void);
/* Nonblocking; returns 1 while the PC-1403 break key is pressed, else 0. */
unsigned char keybrk(void);

#endif
