#pragma org(0xE030)

unsigned int __at(32) seed = 0x1234;
__xram __at(0xE600) unsigned int observed;
__xram __at(0xE602) unsigned char initialized = 0x123;
__xram __at(0xE603) unsigned char zeroed;
__xram __at(0xE604) unsigned int initial_word = 0 - 1;
__xram __at(0xE606) unsigned char passed;

unsigned int increment(unsigned int value)
{
    unsigned int local = value + 1;
    return local;
}

void main(void)
{
    unsigned int i = 0;
    observed = seed;
    while (i < 3) {
        observed = increment(observed);
        i = i + 1;
    }
    if (initialized == 35) {
        if (zeroed == 0) {
            if (initial_word == 65535) passed = 255;
        }
    }
}
