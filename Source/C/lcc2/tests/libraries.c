#include "stdint.h"
#include "compat.h"
#include "stdio.h"
#include "string.h"
#include "int2str.h"
#include "stdint.h"
#include "compat.h"
#include "stdio.h"
#include "string.h"
#include "int2str.h"

__xram __at(0xE600) uint16_t number;
__xram __at(0xE602) char *destination;
__xram __at(0xE604) char *source;
__xram __at(0xE606) byte operation;
__xram __at(0xE608) word output_result;
__xram __at(0xE60A) uint8_t completed;
__xram __at(0xE60B) uint8_t narrow;
__xram __at(0xE60C) word wide;

void main(void)
{
    narrow = 0x1234;
    wide = 0xABCD;
    if (operation == 0) {
        int2str(number, destination);
    } else if (operation == 1) {
        output_result = puts(source);
    } else if (operation == 2) {
        output_result = putchar(number);
    } else {
        int2str(number, destination);
        output_result = puts(destination);
    }
    completed = 42;
}
