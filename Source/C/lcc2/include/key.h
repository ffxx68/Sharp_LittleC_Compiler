#ifndef LCC2_KEY_H
#define LCC2_KEY_H

/* Target-specific raw key codes, not ASCII (PC-1350, PC-1401, PC-1403). */
unsigned char key_wait(void); /* Blocks until a key is pressed. */
/* Nonblocking; returns 0xFF when no key is pressed.
   PC-1350: 0..63, 64 for multiple keys; PC-1401: 0..76; PC-1403: 0..83. */
unsigned char key_scan(void);
/* Nonblocking; returns 1 while the PC-1403 break key is pressed, else 0. */
unsigned char keybrk(void);

#endif
