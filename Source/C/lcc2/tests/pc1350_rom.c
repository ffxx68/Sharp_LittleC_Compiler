#include "pc1350.h"

#pragma org(0x6030)

__xram __at(PC1350_STRING_BUFFER) unsigned char text[PC1350_STRING_CAPACITY]
    = {49, 50, 51};
__xram __at(0x6800) unsigned char number[8];
__xram __at(0x6810) int parsed;
__xram __at(0x6812) int formatted_length;
__xram __at(0x6814) int equal;
__xram __at(0x6816) int unequal;
__xram __at(0x6818) int less;
__xram __at(0x681A) int greater;
__xram __at(0x681C) int less_equal;
__xram __at(0x681E) int greater_equal;
__xram __at(0x6820) int invalid_length;
__xram __at(0x6822) int invalid_operation;
__xram __at(0x6824) int empty_equal;

void main(void)
{
    parsed = pc1350_val(3, &number[0]);
    formatted_length = pc1350_str(&number[0]);
    text[0] = 65;
    text[1] = 66;
    equal = pc1350_compare(PC1350_COMPARE_EQ, 1, 1);
    unequal = pc1350_compare(PC1350_COMPARE_NE, 1, 1);
    less = pc1350_compare(PC1350_COMPARE_LT, 1, 1);
    greater = pc1350_compare(PC1350_COMPARE_GT, 1, 1);
    less_equal = pc1350_compare(PC1350_COMPARE_LE, 1, 1);
    greater_equal = pc1350_compare(PC1350_COMPARE_GE, 1, 1);
    invalid_length = pc1350_val(80, &number[0]);
    invalid_operation = pc1350_compare(6, 1, 1);
    empty_equal = pc1350_compare(PC1350_COMPARE_EQ, 0, 0);
}
