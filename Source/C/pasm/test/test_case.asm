; CASE/ENDCASE tables with forward-referenced value and ELSE targets,
; plus forward labels whose names are prefixes of already-defined labels.
.ORG 57392
	CASE
	0	GRP0
	ELSE	GRPX
	ENDCASE
GRP0:
	CASE
	3	TGT3
	7	TGT7
	ELSE	DFLT
	ENDCASE
	JP	DONE
GRPX:
	CALL	DFLT
	JP	DONE
TGT3:
	LIA	3
	RTN
DFLT12:
	CASE
	1	TGT3
	ELSE	DFLT1
	ENDCASE
	RTN
TGT7:
	LIA	7
	RTN
DFLT1:
	JRNZP	DFLT
	RTN
DFLT:
	LIA	33
	RTN
DONE:
	RTN
