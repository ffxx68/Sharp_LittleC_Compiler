__xram __at(0xE600) unsigned int observed;

unsigned int add(unsigned int left, unsigned char right)
{
    unsigned int local = left;
    return local + right;
}

unsigned int increment(unsigned int value)
{
    return value + 1;
}

unsigned char narrow(unsigned int value)
{
    return value;
}

unsigned int main(void)
{
    observed = add(increment(increment(20)), narrow(22));
    return 0;
}
