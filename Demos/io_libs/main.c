#org 0xE030

// CPU registers are used during program execution,
// so they can't be assumed as static globals
byte regI at 0, regJ at 1;
byte regA at 2, regB at 3; 
word regX at 4, regY at 6;

word xram wTmp = 25873;
byte xram bIdx1;
char xram bTmp1;
char xram cBlank[2] = " ";
char xram cNumber[6];
char xram cPrint[24] = "Hello world!";

#include keycode.h  // PC-1403 specific
#include getkey.h   // PC-1403 specific
#include puts.h     // PC-1403 specific

#include int2str.h
#include strcat.h

main()
{
	puts ( &cPrint );
	bTmp1=getkey();
	
	/*
	for (bIdx1 = 0; bIdx1 <= 3; bIdx1++) {
		cPrint[0] = 0;
		cNumber[0] = 0;
		
		wTmp++;
		
		int2str ( wTmp, &cNumber );
		strcat ( &cPrint, &cNumber );
		puts ( &cPrint );
		bTmp1=getkey();
		
		puts ( &cBlank );
		bTmp1=getkey();
	}
	*/

	// wait a bit, before going to the keycode()
	for (bIdx1 = 0; bIdx1 <= 100; bIdx1++) {
		#asm
			LII 07
		WAITLOOP:
			WAIT 255
			DECI
			JRNZM WAITLOOP
		#endasm
	}
	
	wTmp = 0;
	bIdx1 = 128;
	
	while (bIdx1 == 128)
	{
		bIdx1 = keycode();
		wTmp++;
	}
	
	cNumber[0] = 0;
	cPrint[0] = 0;
	int2str(bIdx1, &cNumber);
	strcat ( &cPrint, &cNumber );
	int2str(wTmp, &cNumber);
	strcat ( &cPrint, &cBlank );
	strcat ( &cPrint, &cNumber );
	puts(&cPrint);
	bTmp1=getkey();

}