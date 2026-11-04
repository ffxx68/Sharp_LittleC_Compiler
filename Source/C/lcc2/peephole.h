#ifndef LCC2_PEEPHOLE_H
#define LCC2_PEEPHOLE_H

/* Returns a newly allocated optimized assembly string, or NULL on failure. */
char *peephole_optimize(const char *assembly);

#endif
