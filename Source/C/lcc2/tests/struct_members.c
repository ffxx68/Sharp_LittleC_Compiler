#pragma org(0xE030)

__at(20) struct InternalRecord {
    unsigned char tag;
    unsigned int value;
} internal_record;

__xram __at(0xE600) struct ExternalRecord {
    unsigned char tag;
    unsigned int value;
} external_record;

__xram __at(0xE700) unsigned int results[3];

unsigned int inspect_external(
    struct ExternalParameter { unsigned char tag; unsigned int value; } *record)
{
    record->tag = 0x7C;
    record->value = 0x1234;
    return record->value;
}

unsigned int main(void)
{
    (&internal_record)->tag = 0xA5;
    (&internal_record)->value = 0xBEEF;
    (&external_record)->tag = 0x7C;
    (&external_record)->value = 0x1234;
    results[0] = (&internal_record)->value;
    results[1] = (&external_record)->tag;
    results[2] = (&external_record)->value;
    return (&external_record)->value;
}
