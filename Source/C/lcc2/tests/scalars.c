#pragma org(0xE030)

unsigned char __at(8) reg_byte;
unsigned int __at(63) reg_word;
__xram __at(0xE600) unsigned int result;
__xram __at(0xE602) unsigned char byte_result;
__xram __at(0xE603) char plain_result;
__xram __at(0xE7FF) unsigned int cross_page;

unsigned int main(void)
{
    unsigned int a = 40;
    unsigned char b = 3;
    signed char s = 0 - 1;
    char c = 255;

    reg_byte = b;
    reg_word = 0x1234;
    result = a + (b = 258);
    byte_result = reg_word;
    plain_result = c;
    cross_page = reg_word + result;
    {
        unsigned int a = 7;
        byte_result = a + byte_result;
    }
    result = result + (s + 1);
    return result;
}
