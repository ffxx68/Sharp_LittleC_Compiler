#include "key.h"
#include "lcd.h"

#pragma org(0x3801)

__xram __at(0x6400) unsigned char lcd_left[40];
__xram __at(0x6440) unsigned char lcd_right[40];
__xram __at(0x4700) unsigned char pixels_ready;

void main(void)
{
    unsigned char x;
    unsigned char mask = 1;
    unsigned char pattern;

    lcd_on();
    for (x = 0; x < 80; x++) {
        pattern = 0x41;
        if ((x & 3) == 0) pattern = pattern | mask;
        if (x == 0 || x == 79) pattern = 0x7F;
        if (x < 40) lcd_left[x] = pattern;
        else lcd_right[79 - x] = pattern;
        mask = mask << 1;
        if (mask == 0x80) mask = 1;
    }
    pixels_ready = 42;
    key_wait();
}
