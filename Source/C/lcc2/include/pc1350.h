#ifndef LCC2_PC1350_H
#define LCC2_PC1350_H

/* PC-1350 ROM workspace, borrowed and overwritten by these non-reentrant APIs.
   Declare an __xram __at(PC1350_STRING_BUFFER) unsigned char buffer[80].
   Payloads have explicit lengths, not NUL terminators. */
#define PC1350_STRING_BUFFER 0x6E60
#define PC1350_STRING_CAPACITY 80

#define PC1350_ROM_UNSUPPORTED (-1)
#define PC1350_ROM_INVALID_ARGUMENT (-2)
#define PC1350_ROM_CONVERSION_ERROR (-3)

#define PC1350_COMPARE_EQ 0
#define PC1350_COMPARE_NE 1
#define PC1350_COMPARE_LT 2
#define PC1350_COMPARE_GT 3
#define PC1350_COMPARE_LE 4
#define PC1350_COMPARE_GE 5

/* Valid 8-byte ROM-format BCD number -> workspace; returns its text length.
   Formatting is BASIC STR$, not C printf. May alter LCD/firmware scratch. */
int pc1350_str(const unsigned char *number);

/* Parse 1..79 printable ASCII bytes in workspace -> 8-byte BCD number.
   Returns 0 on success; destination is unchanged on error.
   Parsing follows BASIC VAL, not strtol (no end pointer). */
int pc1350_val(unsigned char length, unsigned char *number);

/* Compare left then right, packed consecutively in workspace (total <=80).
   Printable ASCII only; empty strings allowed. Returns 0/1 for the selected
   predicate, not strcmp's signed ordering. */
int pc1350_compare(unsigned char operation, unsigned char left_length,
                   unsigned char right_length);

/* All calls return the negative status codes above on failure, preserve caller
   IRAM and the firmware string-buffer pointer, but borrow other firmware scratch.
   Number pointers must address 8 writable/readable bytes outside live stack,
   code, and firmware workspace. ROM v1 checked in MAME; ROM v0 and physical
   devices remain unverified. */

#endif
