#include "peephole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(const char *name, const char *input, const char *expected)
{
    char *actual = peephole_optimize(input);
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s\nExpected:\n%sActual:\n%s",
                name, expected, actual == NULL ? "(null)\n" : actual);
        free(actual);
        exit(1);
    }
    free(actual);
}

int main(void)
{
    check("LIA EXAB becomes LIB",
          "\tLIA\t7\t; value\n\tEXAB\n",
          "\tLIB\t7\t; value\n");
    check("PUSH LIB POP becomes LIB",
          "\tPUSH ; byte\n\tLIB\t4\n\tPOP\n",
          "\tLIB\t4\n");
    check("adjacent EXAB instructions are removed",
          "\tEXAB\n\tEXAB\n\tLIB\t2\n",
          "\tLIB\t2\n");
    check("increment pattern becomes INCA",
          "\tLIB\t1\n\tLP\t3\n\tADM\t; Addition\n\tEXAB\n",
          "\tINCA\n");
    check("decrement pattern becomes DECA",
          "\tLIB\t1\n\tLP\t3\n\tEXAB\n\tSBM\t; Subtraction\n\tEXAB\n",
          "\tDECA\n");
    check("legacy constant subtraction fold becomes INCA",
          "\tLIA\t1\t; Load constant 1\n\tEXAB\n\tLP\t3\n"
          "\tSBM\t\t; Subtraction\n\tEXAB\n",
          "\tINCA\n");
    check("legacy constant addition fold becomes DECA",
          "\tLIA\t1\t; Load constant 1\n\tLP\t3\n"
          "\tADM\t\t; Addition\n\tEXAB\n",
          "\tDECA\n");
    check("non-matching operands and labels are preserved",
          "TARGET:\n\tLIB\t2\n\tLP\t3\n\tADM\n\tEXAB\n",
          "TARGET:\n\tLIB\t2\n\tLP\t3\n\tADM\n\tEXAB\n");
    check("comments prevent matching",
          "\tLIA\t7\n; keep barrier\n\tEXAB\n",
          "\tLIA\t7\n; keep barrier\n\tEXAB\n");
    check("empty input remains empty", "", "");
    puts("PASS: peephole optimizer checks.");
    return 0;
}
