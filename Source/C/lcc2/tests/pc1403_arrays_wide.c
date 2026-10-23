__xram __at(0xF400) unsigned char bytes[258];
__xram __at(0xF600) unsigned int words[130];
__xram __at(0xF800) unsigned int results[4];

unsigned int main(void)
{
    unsigned int i = 256;

    bytes[i] = 0x1FF;
    results[0] = bytes[i];
    i = 128;
    words[i] = 0xBEEF;
    results[1] = words[i];
    results[2] = bytes[i + 128];
    results[3] = words[i + 1];
    return words[i];
}
