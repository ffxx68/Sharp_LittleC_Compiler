#include "lcd.h"

#pragma org(0x6900)

void main(void)
{
    lcd_off();
    lcd_on();
}
