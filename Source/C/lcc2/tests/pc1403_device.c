#include "key.h"
#include "lcd.h"

__xram __at(0xE800) unsigned char scan_empty;
__xram __at(0xE801) unsigned char break_empty;
__xram __at(0xE802) unsigned char scan_pressed;
__xram __at(0xE803) unsigned char waited_key;
__xram __at(0xE804) unsigned char break_pressed;
__xram __at(0xE805) unsigned char break_released;
__xram __at(0xE806) unsigned char completed;

void main(void)
{
    lcd_on();
    lcd_cls(1);
    lcd_cls(0);
    lcd_pset(0, 0, 1);
    lcd_pset(1, 1, 1);
    lcd_hline(118, 2, 5, 1);
    lcd_pset(120, 0, 1);
    lcd_hline(0, 7, 4, 1);
    lcd_off();
    lcd_on();
    scan_empty = key_scan();
    break_empty = keybrk();
    scan_pressed = key_scan();
    while (scan_pressed == 0xFF) {
        scan_pressed = key_scan();
    }
    waited_key = key_wait();
    break_pressed = keybrk();
    while (break_pressed == 0) {
        break_pressed = keybrk();
    }
    while (keybrk() != 0) {
    }
    break_released = keybrk();
    completed = 42;
}
