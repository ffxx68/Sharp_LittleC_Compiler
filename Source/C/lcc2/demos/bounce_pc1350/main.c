#include <key.h>
#include <lcd.h>

#pragma org(0x6030)

__xram __at(0x7000) unsigned char lcd_memory[0x87C];

unsigned char __at(16) x;
unsigned char __at(17) y;
unsigned char __at(18) old_x;
unsigned char __at(19) old_y;
unsigned char __at(20) paddle_x;
unsigned char __at(21) paddle_direction;
unsigned char __at(22) dx;
unsigned char __at(23) dy;
unsigned char __at(24) snake_pos;
unsigned char __at(25) frame;
unsigned char __at(26) wait;
unsigned char __at(27) running;
unsigned char __at(28) snake_x[5];
unsigned char __at(33) snake_y[5];
void main(void)
{
    lcd_on();
    for (x = 0; x < 150; x++) {
        lcd_pset(x, 0, 1);
        lcd_pset(x, 31, 1);
    }
    for (y = 0; y < 32; y++) {
        lcd_pset(0, y, 1);
        lcd_pset(149, y, 1);
    }

    x = 70;
    y = 16;
    dx = 1;
    dy = 1;
    paddle_x = 66;
    paddle_direction = 1;
    for (wait = 0; wait < 9; wait++)
        lcd_pset(paddle_x + wait, 2, 1);
    snake_pos = 0;
    frame = 0;
    running = 1;
    for (wait = 0; wait < 5; wait++) {
        snake_x[wait] = x - wait;
        snake_y[wait] = y;
        lcd_pset(snake_x[wait], snake_y[wait], 1);
    }

    while (running != 0) {
        old_x = snake_x[snake_pos];
        old_y = snake_y[snake_pos];
        wait = old_x - paddle_x;
        if (old_y != 2 || wait >= 9)
            lcd_pset(old_x, old_y, 0);

        if (x == 1) dx = 1;
        if (x == 148) dx = 0;
        if (y == 1) dy = 1;
        if (y == 30) dy = 0;

        wait = x - paddle_x;
        if (dy == 0 && y == 3 && wait < 9) {
            dy = 1;
            if (wait < 4) dx = 0;
            else dx = 1;
        }

        if (dx != 0) x++;
        else x--;
        if (dy != 0) y++;
        else y--;

        lcd_pset(x, y, 1);
        snake_x[snake_pos] = x;
        snake_y[snake_pos] = y;
        snake_pos++;
        if (snake_pos == 5) snake_pos = 0;

        if (paddle_x == 140) paddle_direction = 0;
        if (paddle_x == 1) paddle_direction = 1;
        if (paddle_direction != 0) {
            old_x = paddle_x;
            paddle_x++;
            wait = paddle_x + 8;
        } else {
            old_x = paddle_x + 8;
            paddle_x--;
            wait = paddle_x;
        }
        lcd_pset(old_x, 2, 0);
        lcd_pset(wait, 2, 1);

        frame++;
        if (frame == 4) {
            frame = 0;
            running = key_scan() == 0xFF;
        }

        for (wait = 0; wait < 20; wait++) {
        }
    }
}
