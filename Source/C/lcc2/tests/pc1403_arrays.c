unsigned char __at(12) bytes[3] = {10, 20};
unsigned int __at(16) words[3] = {0x1234, 0x5678};
__xram __at(0xEEFE) unsigned char external_bytes[3] = {7, 8};
__xram __at(0xEFFF) unsigned int external_words[3] = {0xABCD, 0x0102};
__xram __at(0xEC00) unsigned int results[16];
__xram __at(0xEC20) unsigned char index_calls;
__xram __at(0xECFC) unsigned char zero_external[8];
unsigned char __at(22) zero_registers[8];

unsigned int index(void)
{
    index_calls = index_calls + 1;
    return 1;
}

unsigned int main(void)
{
    unsigned char local_bytes[] = {1, 2, 3};
    unsigned int local_words[5] = {0x3456, 0x789A};
    signed char signed_bytes[2] = {0 - 1};
    unsigned int i;

    for (i = 0; i < 3; i = i + 1) {
        bytes[i] = bytes[i] + local_bytes[i];
        words[i] = words[i] + local_words[i];
        external_bytes[i] = external_bytes[i] + bytes[i];
        external_words[i] = external_words[i] + words[i];
        local_bytes[i] = external_bytes[i];
        local_words[i] = external_words[i];
    }
    results[0] = bytes[0];
    results[1] = bytes[1];
    results[2] = bytes[2];
    results[3] = words[0];
    results[4] = words[1];
    results[5] = words[2];
    results[6] = local_bytes[0];
    results[7] = local_bytes[1];
    results[8] = local_bytes[2];
    results[9] = local_words[0];
    results[10] = local_words[1];
    results[11] = local_words[2];
    results[12] = (local_bytes[index()] = 0x1FF) + index_calls;
    i = 0;
    results[13] = signed_bytes[i];
    results[14] = local_words[3];
    results[15] = local_words[4];
    return local_words[index()];
}
