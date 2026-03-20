#org 0xe030


word i;

byte xram stock at 0xFB10;

main()
{

  for (i=0;i<8;i++)
  {

	if (i==0) stock=0xf5;
	if (i==1) stock=0x41;
	if (i==2) stock=0x42;
	if (i==3) stock=0x43;
	if (i==4) stock=0x44;
	if (i==5) stock=0x45;
	if (i==6) stock=0x46;
	if (i==7) stock=0x00;

	#asm
	LIDP 0xFB10
	LDD
	LIDP 0xfbd8
	FILD
	#endasm
  }


}