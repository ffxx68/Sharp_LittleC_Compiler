; stdio adapter for the PC-1403 blocking raw-key endpoint.
getchar:
	CALL	key_wait
	RTN
