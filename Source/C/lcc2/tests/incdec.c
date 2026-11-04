#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a;
    unsigned int b = 3;
    unsigned char c = 250;
    unsigned int i;

    i = 5;
    results[0] = i++;
    results[1] = i;
    results[2] = ++i;
    results[3] = i--;
    results[4] = --i;
    for (i = 0, a = 0; i < 10; i++) {
        a += i;
    }
    results[5] = a;
    results[6] = a++ + 1;
    results[7] = a;
    results[8] = b++;
    results[9] = b;
    results[10] = c++;
    results[11] = c;
    c = 255;
    c++;
    results[12] = c;
    results[13] = (c = 7, c++, c);
    a = 0;
    a--;
    results[14] = a;
    results[15] = --a;
    return a;
}
