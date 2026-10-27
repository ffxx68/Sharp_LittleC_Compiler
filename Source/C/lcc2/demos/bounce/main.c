#include <key.h>
#include <lcd.h>

unsigned char __at(16) key;
unsigned char __at(17) previous_key;
unsigned char __at(18) x;
unsigned char __at(19) y;
unsigned char __at(20) breaking;
unsigned char __at(21) tail_x;
unsigned char __at(22) tail_y;
unsigned char __at(23) cursor_x;
unsigned char __at(24) previous_cursor_x;
unsigned char __at(25) loop_count;
unsigned char __at(26) snake_pos;
unsigned char __at(27) point_x;
unsigned char __at(28) point_y;
unsigned char __at(29) point_mode;
unsigned char __at(30) dx;
unsigned char __at(31) dy;
unsigned char __at(32) random;
unsigned char __at(33) snake_x[4];
unsigned char __at(37) snake_y[4];

void main(void)
{
    cursor_x = 50;
    previous_cursor_x = 50;
    dx = 1;
    dy = 0;
    key = 0xFF;
    previous_key = 0xFF;
    breaking = 0;
    snake_pos = 0;
    x = 50;
    y = 6;
    point_x = 100;
    point_y = 6;
    point_mode = 1;
    loop_count = 0;
    random = 0;
    snake_x[0] = 0;
    snake_x[1] = 0;
    snake_x[2] = 0;
    snake_x[3] = 0;
    snake_y[0] = 0;
    snake_y[1] = 0;
    snake_y[2] = 0;
    snake_y[3] = 0;

    lcd_on();
    lcd_cls(0);
    lcd_hline(cursor_x, 0, 5, 1);

    while (breaking == 0) {
        random++;

        key = key_scan();
        if (key == 0xFF) {
            previous_key = 0xFF;
        } else {
            if (previous_key != key && key == 21 && cursor_x < 100) {
                cursor_x += 5;
                previous_key = key;
            }
            if (previous_key != key && key == 16 && cursor_x > 0) {
                cursor_x -= 5;
                previous_key = key;
            }
            if (previous_cursor_x != cursor_x) {
                lcd_hline(previous_cursor_x, 0, 5, 0);
                lcd_hline(cursor_x, 0, 5, 1);
                previous_cursor_x = cursor_x;
            }
        }

        if (loop_count == 10) {
            tail_x = snake_x[snake_pos];
            tail_y = snake_y[snake_pos];
            lcd_pset(tail_x, tail_y, 0);

            if (x == 0) {
                dx = 1;
            }
            if (x == 99) {
                dx = 0;
            }
            if (y == 1) {
                dy = 1;

                if (random > 128) {
                    if (dx == 0) {
                        dx = 1;
                    } else {
                        dx = 0;
                    }
                }

                if (x >= cursor_x && x < cursor_x + 5) {
                    lcd_pset(point_x, point_y, point_mode);
                    point_x++;
                    if (point_x > 119) {
                        point_x = 100;
                        point_y--;
                        if (point_y == 0) {
                            point_y = 6;
                            if (point_mode == 1) {
                                point_mode = 0;
                            } else {
                                point_mode = 1;
                            }
                        }
                    }
                }
            }
            if (y == 6) {
                dy = 0;
            }

            if (dx == 1) {
                x++;
            } else {
                x--;
            }
            if (dy == 1) {
                y++;
            } else {
                y--;
            }

            lcd_pset(x, y, 1);
            snake_x[snake_pos] = x;
            snake_y[snake_pos] = y;

            loop_count = 0;
            snake_pos++;
            if (snake_pos == 4) {
                snake_pos = 0;
            }
        }

        loop_count++;
        breaking = keybrk();
    }
}
