#pragma org(0xE030)

unsigned char __at(8) register_byte;
unsigned int __at(10) register_word;
signed char __at(12) signed_register_byte;
__xram __at(0xE610) unsigned char xram_byte;
__xram __at(0xE5FF) unsigned int xram_word;
__xram __at(0xE700) unsigned int results[8];

unsigned char modify_byte(unsigned char *pointer)
{
    *pointer = 0xA5;
    return *pointer;
}

unsigned int modify_word(unsigned int *pointer)
{
    *pointer = 0x1234;
    return *pointer;
}

int read_signed(signed char *pointer)
{
    return *pointer;
}

unsigned int main(void)
{
    unsigned char *byte_pointer = &register_byte;
    unsigned int *register_word_pointer = &register_word;

    results[0] = (*byte_pointer = 0x107);
    results[1] = register_byte;
    register_word = (*register_word_pointer = 0xBEEF);
    results[2] = register_word;
    results[3] = modify_byte(&xram_byte);
    results[4] = modify_word(&xram_word);
    signed_register_byte = 0x80;
    results[5] = read_signed(&signed_register_byte);
    results[6] = results[0] + results[1] + results[2] +
                 results[3] + results[4] + results[5];
    results[7] = *(&xram_word);
    return results[3];
}
