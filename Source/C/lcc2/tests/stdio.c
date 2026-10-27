#include "stdio.h"

__xram __at(0xE600) unsigned int key_code;
__xram __at(0xE602) unsigned int output_result;
__xram __at(0xE604) unsigned char completed;

void main(void)
{
    key_code = getchar();
    output_result = putchar('A');
    completed = 42;
}
