puts(word wAddr1)
{
	
	regX = wAddr1; // destination address
#asm
	call LIB_PUTS
#endasm

	return;
	
// using routines from PC-1403 ROM
#asm
.include _puts.lib
#endasm

}