#org 0xE030

#include common.h

char xram bTmp1;
byte xram bXPos, bYPos, bDx, bDy;
word xram wTmp1;
char xram cHelloWorld[25] = "Hello!";

#include lcd.h // PC-1403 specific
#include getkey.h // PC-1403 specific
#include puts.h // PC-1403 specific

main()
{
	
	// print string
	lcd_cls(0);
	puts(&cHelloWorld);
	bTmp1=getkey();
	wTmp1 = 0;
	bXPos = 0;
	bYPos = 0;

	// fill
	lcd_cls(1);
	bTmp1=getkey();
	
	// horizontal lines
	lcd_cls(0);
	lcd_hline(10,0,100,1);
	lcd_hline(12,2,100,1);
	lcd_hline(14,4,100,1);
	bTmp1=getkey();

	// zig-zag
	lcd_cls(0);
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
	bTmp1=getkey();
	
}