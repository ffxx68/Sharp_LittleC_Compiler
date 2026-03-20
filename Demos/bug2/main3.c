// this program displays Hello World
// but the tab16 array does not contain the values
// it should contain, why?

#org 0xE030

#include common.h

char xram bTmp1;
char xram cHello[24] = "Hello world!";


word xram tab16[8] at 0xFBD8 =(1,2,3,4,5,6,7,8);
word xram i;
byte tab8[16]=(9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24);

#include puts.h  // PC-1403 specific; put a string on screen
#include key.h // PC-1403 specific; get keycode (stopping execution)

main()
{

	puts ( &cHello );
	bTmp1=key_wait();

	for (i = 0; i < 8; i++) {
        tab16[i] = (tab8[i * 2] << 8) | tab8[i * 2 + 1];
    }

	puts ( &cHello );
	bTmp1=key_wait();



}