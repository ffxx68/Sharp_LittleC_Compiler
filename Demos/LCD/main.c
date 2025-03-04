#org 0xE030
/*
Reserving 2 K on top of BASIC program memory:

PC-1403

NEW
MEM
 6878
POKE &FF01,&30,&E8 
NEW
MEM
 4830

 0xE030 to 0xE82F for ASM program (1865 bytes) 
 0xE830 ... BASIC
 
Machine code entry point at 0xE0E8 = 57576

*/

#define LCD_LEFT 0x3000
#define LCD_RIGHT 0x306C

byte regI at 0, regJ at 1;
byte regA at 2, regB at 3; 
word regX at 4, regY at 6;
byte regK at 8;

char xram bTmp1;
byte xram bXPos, bYPos, bDx, bDy;
word xram wTmp1;
char xram cHelloWorld[25] = "Hello!";

char readbyte(word adr)
{
	// overwriting X!
	regX = adr;
#asm
	DX
	IXL
#endasm
}

writebyte(word adr2, char byt)
{
	// overwriting Y!
	regY = adr2;
	regA = byt;
#asm
	DY
	IYS
#endasm
  return;
}
 
// using display routines from ROM (PC-1403)
#include lcd.h // PC-1403 specific
#include getkey.h // PC-1403 specific
#include puts.h // PC-1403 specific

main()
{
	
	lcd_cls(0);
	puts(&cHelloWorld);
	bTmp1=getkey();

	wTmp1 = 0;
	bXPos = 0;
	bYPos = 0;
	while (wTmp1<1000) {
		lcd_pset(  bXPos, bYPos, 1 );
		if (bXPos == 0) bDx=1 ;
		if (bXPos == 117) bDx=0 ;
		if (bYPos == 0) bDy=1 ;
		if (bYPos == 7) bDy=0 ;
		if (bDx == 1) bXPos=bXPos+1 ; else bXPos=bXPos-1 ;
		if (bDy == 1) bYPos=bYPos+1 ; else bYPos=bYPos-1 ;
		wTmp1++;
	}
	lcd_cls(1);
	bTmp1=getkey();

}