#org 0xE030

// CPU registers , etc.
// required here as reused by some libraries too
#include common.h

word xram wTmp = 25873;
byte xram bIdx1, bIdx2;
char xram bTmp1;
char xram cBlank[2] = " ";
char xram cNumber[6];
char xram cPrint[24] = " ";

#include keycode.h  // PC-1403 specific
#include getkey.h   // PC-1403 specific
#include puts.h     // PC-1403 specific

#include int2str.h
#include string.h

main()
{
	//strcpy ( &cPrint, &cBlank, 1 );
	cPrint[0] = 0;
	wTmp = 0;
	bIdx1 = 0;
	bIdx2 = 0;
	bTmp1 = 0;
	while (bTmp1 == 0)
	{
		// wait
		#asm
			LII 01   
		WAITLOOP2:
			WAIT 255 
			DECI
			JRNZM WAITLOOP2
		#endasm
		
		bIdx1 = keycode();
		if (bIdx2 != bIdx1 ) { // anti-repeat
			if (bIdx1 == 44 || bIdx1 == 45 || bIdx1 == 46 ) { // Z A Q (left)
				wTmp--;
			}
			if (bIdx1 == 13 || bIdx1 == 14 || bIdx1 == 15 ) { // SPC K I (right)
				wTmp++;
			}
			bIdx2 = bIdx1;
		}
		
		
		//  . . .
		
		
		bTmp1 = keybrk(); // BRK to exit
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