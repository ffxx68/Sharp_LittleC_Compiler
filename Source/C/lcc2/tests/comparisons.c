#pragma org(0xE030)

__xram __at(0xF000) unsigned int left;
__xram __at(0xF002) unsigned int right;
__xram __at(0xF010) unsigned char equal_result;
__xram __at(0xF011) unsigned char unequal_result;
__xram __at(0xF012) unsigned char less_result;
__xram __at(0xF013) unsigned char less_equal_result;
__xram __at(0xF014) unsigned char greater_result;
__xram __at(0xF015) unsigned char greater_equal_result;

void main(void)
{
    equal_result = left == right;
    unequal_result = left != right;
    less_result = left < right;
    less_equal_result = left <= right;
    greater_result = left > right;
    greater_equal_result = left >= right;
    equal_result = left == right;
}
