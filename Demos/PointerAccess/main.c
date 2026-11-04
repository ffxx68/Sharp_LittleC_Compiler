#org 0x8000

byte byteTarget at 10;
word wordTarget at 20;
byte xram xramByteTarget;
word xram xramWordTarget;
byte *bytePointer;
word *wordPointer;
word *xram wordXramPointer;

main()
{
    bytePointer = &xramByteTarget;
    byteTarget = *bytePointer;
    *bytePointer = 1;

    wordPointer = &wordTarget;
    wordTarget = *wordPointer;
    *wordPointer = 2;

    bytePointer = &byteTarget;
    byteTarget = *bytePointer;
    *bytePointer = 3;

    wordXramPointer = &xramWordTarget;
    wordTarget = *wordXramPointer;
    *wordXramPointer = 4;
}
