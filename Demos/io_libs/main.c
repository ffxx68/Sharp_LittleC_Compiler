#org 0xE030

// CPU registers , etc.
// required here as reused by some libraries too
#include common.h

byte xram bKey1, bKey2, bTmp1, bCpos, bCpos_old, bLoopNr;
byte xram bXball, bYball, bDxBall, bDyBall;
//word xram wTmp1;
//char xram cPtr1[6] = "X";

#include lcd.h  // PC-1403 specific
#include key.h  // PC-1403 specific
//#include string.h
//#include puts.h   // PC-1403 specific

main()
{
	//bCpos = 0; // initial cursor position
	//bCpos_old = 0;
	bKey1 = 0xFF; // no key
	bKey2 = 0xFF; // no key
	bYball = 1;
	bTmp1 = 0;
	// bTmp1 = 0;

	lcd_on();
	lcd_cls(0);
	lcd_hline(bCpos, 0, 5, 1);

	/* draw grid
	for (bTmp1 = 0; bTmp1<120; bTmp1+=5)
	{
		lcd_pset(bTmp1-5, 1, 1);
		lcd_pset(bTmp1-5, 2, 1);
	}		
	bTmp1 = 0;
	*/
	
	// main loop
	while (bTmp1 == 0)
	{
		bLoopNr++;
		
		
		// keyboard scan and cursor update
		bKey1 = key_scan(); 
		if (bKey1 == 0xFF) { // no key pressed
			bKey2 = 0xFF;
		} else {
			if (bKey2 != bKey1 && bKey1 == 21 && bCpos < 100) { // > : move right 
				bCpos+=5;
				bKey2 = bKey1; // anti-repeat
			}
			if (bKey2 != bKey1 && bKey1 == 16 && bCpos > 0) { // < : move left 
				bCpos-=5;
				bKey2 = bKey1; // anti-repeat
			}
			if (bCpos_old != bCpos) {
				// draw cursor in new position
				lcd_hline(bCpos_old, 0, 5, 0);
				lcd_hline(bCpos, 0, 5, 1);
				bCpos_old = bCpos;
			}
		}
		
		
		// bouncing ball
		if (bLoopNr == 10) { // delay
			lcd_pset(  bXball, bYball, 0 );
			if (bXball == 0) bDxBall=1 ;
			if (bXball == 100) bDxBall=0 ;
			if (bYball == 1) bDyBall=1 ;
			if (bYball == 7) bDyBall=0 ;
			if (bDxBall == 1) bXball=bXball+1 ; else bXball=bXball-1 ;
			if (bDyBall == 1) bYball=bYball+1 ; else bYball=bYball-1 ;
			lcd_pset(  bXball, bYball, 1 );
			bLoopNr = 0;
		}
		
		
		// ...
		
		
		// BRK to exit
		bTmp1 = keybrk(); 
	}

}

