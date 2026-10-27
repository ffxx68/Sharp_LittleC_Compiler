#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[16];
__xram __at(0xE760) unsigned int data[2];
__xram __at(0xE790) struct Record {
    unsigned char tag;
    unsigned int value;
} record;

unsigned int main(void)
{
    unsigned int i = 1;

    (&record)->value = 1000;
    (&record)->value += 234;
    (&record)->tag = 9;
    (&record)->tag++;
    results[0] = (&record)->value;
    results[1] = (&record)->tag;
    data[0] = 50;
    data[1] = 60;
    results[2] = data[i - 1]--;
    results[3] = data[0];
    results[4] = (data[i] += 3);
    results[5] = (&record)->value++ + 1;
    results[6] = (&record)->value;
    return results[4];
}
