#org 0xE030

// CPU registers are used during program execution,
// so they can't be assumed as static global variable
byte regI at 0, regJ at 1;
byte regA at 2, regB at 3; 
word regX at 4, regY at 6;

word xram wTmp = 25873;
byte xram bIdx1;
char xram bTmp1;
char xram cBlank[2] = " ";
char xram cNumber[6];
char xram cPrint[24] = "Hello world!";

#include puts.h  // PC-1403 specific
#include int2str.h
#include getkey.h // PC-1403 specific
#include strcat.h

main()
{
	puts ( &cPrint );
	bTmp1=getkey();
	for (bIdx1 = 0; bIdx1 <= 3; bIdx1++) {
		cPrint[0] = 0;
		cNumber[0] = 0;
		
		int2str ( wTmp, &cNumber );
		strcat ( &cPrint, &cNumber );
		puts ( &cPrint );
		bTmp1=getkey();
		
		puts ( &cBlank );
		bTmp1=getkey();
	}
	
}