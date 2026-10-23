unsigned char __at(8) register_byte = 0x123;
unsigned char __at(9) character_byte = 'A';
unsigned char __at(10) zero_register_byte;
__xram __at(0xE600) signed int xram_word = 0 - 1;
__xram __at(0xE602) unsigned char zero_byte;

void main(void)
{
}
