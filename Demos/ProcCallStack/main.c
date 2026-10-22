byte resultByte;
word resultWord;

byte retByte3(byte rbA, byte rbB, byte rbC)
{
    byte rbLocal;
    return rbA;
}

word retWord4(word rwA, word rwB)
{
    return rwA;
}

noReturn3(byte nr3A, word nr3B)
{
    resultWord = nr3B;
}

noReturn4(word nr4A, word nr4B)
{
    resultWord = nr4A;
}

local7(byte l7P)
{
    byte l7A;
    byte l7B;
    byte l7C;
    word l7D;
    word l7E;
    resultByte = l7P;
}

local8(byte l8P)
{
    float l8F;
    resultByte = l8P;
}

consumeFloat(float cfP)
{
}

word nested(word nxX, byte nxY)
{
    resultByte = retByte3(nxY, 1, 2);
    return retWord4(nxX, 20);
}

main()
{
    resultByte = retByte3(1, 2, 3);
    resultWord = retWord4(10, 20);
    noReturn3(1, 2);
    noReturn4(3, 4);
    local7(5);
    local8(6);
    consumeFloat(1.25);
    resultWord = nested(7, 8);
}
