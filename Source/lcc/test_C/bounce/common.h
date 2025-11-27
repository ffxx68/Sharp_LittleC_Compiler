// common definitions
byte regI at 0, regJ at 1;
byte regA at 2, regB at 3; 
word regX at 4, regY at 6;
byte regK at 8, regL at 9, regM at 10;

char readbyte(word adr1)
{
	regX = adr1;
#asm
	DX
	IXL
	RTN
#endasm
}

writebyte(word adr2, char byt2)
{
	regY = adr2;
	regA = byt2;
#asm
	DY
	IYS
	RTN
#endasm
}