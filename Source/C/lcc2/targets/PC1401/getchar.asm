; stdio adapter for the PC-1401 blocking raw-key endpoint.
getchar:
	CALL	key_wait
	RTN
