#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a = 1234;
    unsigned int b = 7;
    unsigned int z = 0;
    unsigned int big = 40000;
    unsigned char c = 200;
    unsigned int d = 300;

    results[0] = a * b;
    results[1] = a / b;
    results[2] = a % b;
    results[3] = big * 3;
    results[4] = c * c;
    results[5] = big / 256;
    results[6] = big % 256;
    results[7] = z * b;
    results[8] = z / b;
    results[9] = b / a;
    results[10] = b % a;
    results[11] = a / z;
    results[12] = a % z;
    results[13] = (a + 1) * 2 / (b - 4) % 100;
    results[14] = d * d + d / 7 % 5;
    results[15] = 0xFFFF / 0x10 + 1 * 0 + 255 % 16;
    return a / b;
}
