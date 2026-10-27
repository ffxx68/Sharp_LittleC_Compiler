; PC-1403 blocking input: return the raw hardware key code in B:A.
getchar:
	CALL	0x1494
	LIDP	0xFF5E
	LDD
	LIB	0
	RTN
