; stdio adapter returning a zero-extended raw PC-1350 key code, not ASCII.
getchar:
	CALL	key_wait
	RTN
