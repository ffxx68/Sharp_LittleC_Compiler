#ifndef LCC2_KEY_H
#define LCC2_KEY_H

/* PC-1403 raw key codes, not ASCII. */
unsigned char key_wait(void); /* Blocks until a key is pressed. */
/* Nonblocking; returns 0xFF when no key is pressed (key codes are 0..83). */
unsigned char key_scan(void);
/* Nonblocking; returns 1 while the PC-1403 break key is pressed, else 0. */
unsigned char keybrk(void);

#endif
