#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a = 0x1234;
    unsigned int b = 0x8001;
    unsigned int w = 0x100;
    unsigned char n = 4;
    unsigned char zero = 0;
    unsigned char n16 = 16;
    unsigned char c = 0x81;

    results[0] = a << 4;
    results[1] = a >> 4;
    results[2] = a << n;
    results[3] = a >> n;
    results[4] = a << zero;
    results[5] = a >> zero;
    results[6] = a << 8;
    results[7] = a >> 8;
    results[8] = b << 1;
    results[9] = b >> 15;
    results[10] = b << 15;
    results[11] = b >> 1;
    results[12] = a << n16;
    results[13] = a >> w;
    results[14] = c << 1;
    results[15] = (a >> 4) + (1 << 3);
    return a >> n;
}
