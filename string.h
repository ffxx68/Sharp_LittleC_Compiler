// convert integer (16bit) to a string
int2str( word wNumber, word wNumStr ) 
{
	regX = wNumber; // input
	regY = wNumStr; // destination 
#asm
	call INT2STR_LIB
#endasm
   return;
   
#asm  
   .include _int2str.lib
#endasm
}

// beware - no string end or allocation check!
strcat(word wDest1, word wSrc1)
{
	regX = wSrc1;
	regY = wDest1;
#asm
	call LIB_STRCAT
#endasm
	return;
#asm
.include _strcat.lib
#endasm
}

// beware - no string end or allocation check!
strcpy(word wDest2, word wSrc2, byte bStrSize1) 
{
	regX = wSrc2;
	regY = wDest2;
	regI = bStrSize1;
#asm
	DX
	DY
LIB_STRCPY_LOOP:
	DECI
	JRZP LIB_STRCPY_END
	IXL
	IYS
	CPIA 0
	JRNZM LIB_STRCPY_LOOP
LIB_STRCPY_END:
	RTN
#endasm
}

// TODO ...
//strcmp(word wStrCmp1, word wStrCmp2, byte bStrSize2) 
//strncmp(word wStrCmp1, word wStrCmp2, byte bStrSize2) 