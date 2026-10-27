#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];
__xram __at(0xE760) unsigned int data[4];
__xram __at(0xE780) unsigned char bytes[4];

unsigned int main(void)
{
    unsigned int i;
    unsigned int *wp;
    unsigned char *bp;

    data[0] = 100;
    data[1] = 200;
    data[0] += 11;
    data[1]++;
    i = 1;
    data[i] -= 1;
    ++data[i - 1];
    results[0] = data[0] + data[1];

    wp = &data[0];
    *wp += 1;
    wp++;
    (*wp)++;
    wp += 1;
    *wp = 7;
    wp -= 2;
    results[1] = *wp + data[1] + data[2];

    bytes[0] = 1;
    bytes[1] = 2;
    bytes[2] = 3;
    bytes[3] = 4;
    bp = &bytes[0];
    bp += 2;
    (*bp)++;
    results[2] = bytes[2];
    return results[2];
}
