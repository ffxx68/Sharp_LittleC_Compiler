#include "string.h"
#include "string.h"

__xram __at(0xE610) char *source_pointer;
__xram __at(0xE612) char *destination_pointer;
__xram __at(0xE614) char *result_pointer;
__xram __at(0xE616) unsigned char append_mode;
__xram __at(0xE617) unsigned char completed;
__xram __at(0xE618) unsigned char first_byte;

char *transfer(char *destination, const char *source, unsigned char append)
{
    char *result;
    if (append) {
        result = strcat(destination, source);
    } else {
        result = strcpy(destination, source);
    }
    return result;
}

void main(void)
{
    result_pointer = transfer(destination_pointer, source_pointer, append_mode);
    first_byte = *result_pointer;
    completed = 42;
}
