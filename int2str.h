int2str ( word wNumber, word wNumStr ) 
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
