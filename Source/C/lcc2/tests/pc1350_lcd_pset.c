#include <key.h>
#include <lcd.h>

#pragma org(0x6030)

__xram __at(0x7000) unsigned char lcd_memory[0x87C];
__xram __at(0x6810) unsigned char pset_ready;

void main(void)
{
    unsigned char bank;
    unsigned char column;

    lcd_on();
    for (bank = 0; bank < 5; bank++) {
        for (column = 0; column < 30; column++) {
            lcd_memory[bank * 0x200 + column] = 0;
            lcd_memory[bank * 0x200 + 0x40 + column] = 0;
            lcd_memory[bank * 0x200 + 0x1E + column] = 0;
            lcd_memory[bank * 0x200 + 0x5E + column] = 0;
        }
    }

    lcd_pset(0, 0, 1);
    lcd_pset(0, 1, 1);
    lcd_pset(29, 7, 1);
    lcd_pset(30, 8, 1);
    lcd_pset(149, 31, 1);
    lcd_pset(0, 0, 0);
    lcd_pset(150, 0, 1);
    lcd_pset(0, 32, 1);
    lcd_pset(1, 1, 2);

    pset_ready = 42;
    key_wait();
}
