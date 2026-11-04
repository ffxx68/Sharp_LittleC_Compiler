#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];

unsigned int main(void)
{
    unsigned int a = 0x0FF0;
    unsigned int b = 0x1234;
    unsigned char c = 0xAA;

    results[0] = a & b;
    results[1] = a | b;
    results[2] = a ^ b;
    results[3] = ~a;
    results[4] = -b;
    results[5] = !a;
    results[6] = !(a - a);
    results[7] = c ^ 0xFF;
    results[8] = c & 0x0F;
    results[9] = c | 0x100;
    results[10] = -1;
    results[11] = ~0;
    results[12] = +a;
    results[13] = (a & b) | (a ^ b);
    results[14] = (a | 0xF000) ^ 0x00FF;
    results[15] = !!b;
    return a & b;
}
