#pragma org(0xE030)

__xram __at(0xE700) unsigned char discarded_byte;
__xram __at(0xE701) unsigned int discarded_word;
__xram __at(0xE703) unsigned char nested_byte;
__xram __at(0xE704) unsigned int nested_word;
__xram __at(0xE706) unsigned char comma_byte;
__xram __at(0xE707) unsigned int comma_word;
__xram __at(0xE709) unsigned char call_count;
__xram __at(0xE70A) unsigned char wide_byte;
__xram __at(0xE70B) unsigned int wide_word;

unsigned char next_byte(void)
{
    call_count = call_count + 1;
    return 0xA5;
}

unsigned int next_word(void)
{
    call_count = call_count + 1;
    return 0x1234;
}

void main(void)
{
    discarded_byte = next_byte();
    discarded_word = next_word();
    nested_word = nested_byte = next_byte();
    (comma_byte = next_byte(), comma_word = next_word());
    wide_word = wide_byte = next_word();
}
