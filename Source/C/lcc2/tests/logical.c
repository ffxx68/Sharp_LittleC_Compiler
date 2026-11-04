#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a = 0;
    unsigned int b = 5;
    unsigned int c = 0x100;
    unsigned int t = 0;
    unsigned int count = 0;

    results[0] = a && b;
    results[1] = b && c;
    results[2] = a || b;
    results[3] = a || a;
    results[4] = c && 1;
    t = 0;
    a && (t = 1);
    results[5] = t;
    t = 0;
    b || (t = 1);
    results[6] = t;
    t = 0;
    b && (t = 9);
    results[7] = t;
    t = 0;
    a || (t = 3);
    results[8] = t;
    results[9] = (t = 4, b + 1);
    results[10] = (a, b, c);
    if (a || b && c) {
        results[11] = 1;
    } else {
        results[11] = 2;
    }
    t = 3;
    while ((t = t - 1, t)) {
        count = count + 1;
    }
    results[12] = count;
    results[13] = !(a && b);
    results[14] = (b && c) + (b || a);
    results[15] = b > 3 && c > 255;
    return b && c;
}
