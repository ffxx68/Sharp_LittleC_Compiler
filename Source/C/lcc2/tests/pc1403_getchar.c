int getchar(void);

__xram __at(0xE600) unsigned int key_codes[3];
__xram __at(0xE606) unsigned char completed;

void main(void)
{
    unsigned int index;
    for (index = 0; index < 3; index++) {
        key_codes[index] = getchar();
    }
    completed = 42;
}
