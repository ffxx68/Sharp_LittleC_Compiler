#ifndef LCC2_STDIO_H
#define LCC2_STDIO_H

/* PC-1403: blocking raw hardware key code, not ASCII. */
int getchar(void);
/* PC-1403: append printable low byte to 24 columns; -1 if full/unsupported.
   No newline, scrolling or reset API; buffer starts empty on binary load. */
int putchar(int character);

#endif
