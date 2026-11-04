#ifndef LCC2_INT2STR_H
#define LCC2_INT2STR_H

/* Unsigned decimal 0..65535 plus NUL; destination needs at least 6 bytes. */
void int2str(unsigned int number, char *destination);

#endif
