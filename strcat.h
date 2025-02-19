strcat(word wDest, word wSrc) 
{
	
	regX = wSrc;
	regY = wDest;
#asm
	call LIB_STRCAT
#endasm
	return;

#asm
.include _strcat.lib
#endasm

}