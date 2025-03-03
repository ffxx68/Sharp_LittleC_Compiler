#org 0xE030

// CPU registers are used during program execution,
// so they can't be assumed as static global variable
byte regI at 0, regJ at 1;
byte regA at 2, regB at 3; 
word regX at 4, regY at 6;

word xram quotient1;
word xram dividend1;
word xram divisor1;

char xram bTmp1;
char xram cDivSign[5] = "/";
char xram cEqSign[5] = "=";
char xram cHello[24] = "Hello world!";
char xram cNumberBuf[6];
char xram cPrintBuf[24];

#include puts.h  // PC-1403 specific; put a string on screen
#include int2str.h
#include getkey.h // PC-1403 specific; get keycode (stopping execution)
#include strcat.h

main()
{

	puts ( &cHello );
	bTmp1=getkey();
	
	dividend1 = 25872;
	divisor1 = 32;
	
	cNumberBuf[0] = 0;
	cPrintBuf[0] = 0;
	int2str ( dividend1, &cNumberBuf );
	strcat ( &cPrintBuf, &cNumberBuf );
	
	cNumberBuf[0] = 0;
	int2str ( divisor1, &cNumberBuf );
	strcat ( &cPrintBuf, &cDivSign );
	strcat ( &cPrintBuf, &cNumberBuf );

	dividend1++;
	quotient1 = dividend1 / divisor1 ; // 16-bit division 
	
	cNumberBuf[0] = 0;
	int2str ( quotient1, &cNumberBuf );
	strcat ( &cPrintBuf, &cEqSign );
	strcat ( &cPrintBuf, &cNumberBuf );
	
	puts ( &cPrintBuf );
	bTmp1=getkey();

}

