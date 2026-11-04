#ifndef LCC2_STRING_H
#define LCC2_STRING_H

/* NUL-terminated strings; destination capacity and nonoverlap are caller duties. */
char *strcpy(char *destination, const char *source);
char *strcat(char *destination, const char *source);

#endif
