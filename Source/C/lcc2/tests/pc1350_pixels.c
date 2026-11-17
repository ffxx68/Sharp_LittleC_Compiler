#include "key.h"
#include "lcd.h"

#pragma org(0x6030)

__xram __at(0x7000) unsigned char lcd_memory[0x87C];
__xram __at(0x6810) unsigned char pixels_ready;

void main(void)
{
    unsigned char bank;
    unsigned char column;
    unsigned char row;
    unsigned int x;
    unsigned char pattern;
    unsigned int offset;

    lcd_on();
    x = 0;
    for (bank = 0; bank < 5; bank++) {
        for (column = 0; column < 30; column++) {
            for (row = 0; row < 4; row++) {
                if (row == 0) offset = 0;
                if (row == 1) offset = 0x40;
                if (row == 2) offset = 0x1E;
                if (row == 3) offset = 0x5E;
                offset = offset + bank * 0x200 + column;
                pattern = 0;
                if (x >= 10 && x < 50) {
                    if ((x & 4) == 0) pattern = 0x55;
                    else pattern = 0xAA;
                }
                if (x >= 60 + row * 8 && x < 68 + row * 8)
                    pattern = pattern | (1 << ((x - 60) & 7));
                if (row == 0) pattern = pattern | 1;
                if (row == 3) pattern = pattern | 0x80;
                if (x == 0 || x == 149) pattern = 0xFF;
                lcd_memory[offset] = pattern;
            }
            x++;
        }
    }
    pixels_ready = 42;
    key_wait();
}
