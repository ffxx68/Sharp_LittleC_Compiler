#org 0xE030

// CPU registers , etc.
// required here as reused by some libraries too

#include common.h
#include lcd.h  // PC-1403 specific
#include key.h  // PC-1403 specific

byte xram bKey1, bKey2, bTmpX, bTmpY, bBrk;
byte xram bCpos, bCpos_old, bLoopNr;
//byte xram bSnakePos;
byte xram bPoints;
byte xram bDxBall, bDyBall;
byte xram bRnd;
//byte xram bXSnake[6] = (0,0,0,0,0,0);
//byte xram bYSnake[6] = (0,0,0,0,0,0);

main()
{
	bCpos = 50; // initial cursor position
	bCpos_old = 50;

	bKey1 = 0xFF; // no key
	bKey2 = 0xFF; // no key
	bBrk = 0;
	
	//bSnakePos = 0;
	bTmpX = 50; 
	bTmpY = 7;
	//bYSnake[bSnakePos] = bTmpY;
	//bXSnake[bSnakePos] = bTmpX;
	bLoopNr = 0;
	
	bPoints = 0;
	
	lcd_on();
	lcd_cls(0);
	lcd_hline(bCpos, 0, 5, 1);
	
	// main loop
	while (bBrk == 0)
	{
		
// quasi-random number generation, based on timer check		
#asm
		TEST  02 ; 2 ms timer
		JRNZP RND_SKIP
		LIDP  bRnd
		LDD
		INCA
		STD
RND_SKIP:		
		NOP
#endasm
		
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
		
			// clear current position
			//bTmpX = bXSnake[bSnakePos]; 
			//bTmpY = bYSnake[bSnakePos];
			lcd_pset(bTmpX, bTmpY, 0);
			
			// boundary check and direction update
			if (bTmpX == 0) bDxBall=1;
			if (bTmpX == 100) bDxBall=0;
			if (bTmpY == 1) {
				bDyBall=1;
				
				// random inversion of X direction on Y bounce
				if (bRnd > 128) 
					if (bDxBall == 0) bDxBall = 1; else bDxBall = 0;
				
				// check cursor hit and update points
				if (bTmpX>=bCpos && bTmpX<(bCpos+5)) {
					bPoints++;
					if (bPoints > 10) { 
						bPoints=1;
						lcd_hline(101, 7, 10, 0); 
					}
					lcd_pset(101+bPoints, 7, 1);
				}
			}
			if (bTmpY == 7) bDyBall=0 ;
			
			// calc new position
			if (bDxBall == 1) bTmpX=bTmpX+1 ; else bTmpX=bTmpX-1;
			if (bDyBall == 1) bTmpY=bTmpY+1 ; else bTmpY=bTmpY-1;
			
			// draw new position
			lcd_pset(bTmpX, bTmpY, 1);
			//bXSnake[bSnakePos] = bTmpX; 
			//bYSnake[bSnakePos] = bTmpY;
			
			bLoopNr = 0;
			//bSnakePos++;
			//if (bSnakePos == 2) bSnakePos = 0;
		}
		
		bLoopNr++;
		
		// BRK to exit
		bBrk = keybrk(); 
	}

}

