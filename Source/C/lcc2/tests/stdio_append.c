#include "stdio.h"

__xram __at(0xE600) unsigned int key_code;
__xram __at(0xE602) unsigned int output_result;
__xram __at(0xE604) unsigned char completed;
__xram __at(0xE606) unsigned int second_key;
__xram __at(0xE608) unsigned int full_result;
__xram __at(0xE60A) unsigned int control_result;
__xram __at(0xE610) unsigned char return_values[24];

void main(void)
{
    unsigned int index;
    key_code = getchar();
    for (index = 0; index < 24; index++) {
        output_result = putchar(65 + index);
        return_values[index] = output_result;
        if (index == 2) {
            second_key = getchar();
        }
    }
    full_result = putchar('Y');
    control_result = putchar('\n');
    completed = 42;
}
