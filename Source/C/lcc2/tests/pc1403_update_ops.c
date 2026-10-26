#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a = 10;
    unsigned int b = 3;
    unsigned char c = 250;
    unsigned int i;

    a += 5; a -= b; a *= b; a /= 4; a %= 5;
    a <<= 3; a >>= 1; a |= 0x101; a &= 0xF1; a ^= 0xFF;
    results[0] = a;
    c += 10;
    results[1] = c;
    c -= 5;
    results[2] = c;
    results[3] = (b -= 1);
    results[4] = (a += 5) * 2;
    results[5] = (a >>= 1);
    return a;
}
