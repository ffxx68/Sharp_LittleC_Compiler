// NON Blocking key code read (PC-1403)
// mostly copied from PETOOOLS 1.1 (thanks Edgar!)

////////////////////////////////////
// Seems like not working...
// ... to be debugged
////////////////////////////////////

//
// keycode()
// Reads the key code off the whole keyboard (except the BRK key) like the system calls CALL &1494 and CALL &14BF. 
// The difference is, that the control is returned to the calling program, if there is no key pressed. 
// 
// The keys are assigned to the following key codes:
// 
//        SHARP  POCKET COMPUTER PC-1403H  32KB                 +---+ +---+ +---+ +---+ +---+ +---+ 
//                                                              |hyp| |sin| |cos| |tan| |FSE| |CCE| 
//   on                                                         +---+ +---+ +---+ +---+ +---+ +---+ 
//  +-+ +----------------------------------------------------+    67    66    65    64    4     6   
//  ¦ ¦ ¦                                    DEG             |  +---+ +---+ +---+ +---+ +---+ +---+ 
//  +-+ ¦ >                                                  |  |HEX| |DEG| | ln| |log| |1/x| | ^ | 
//  off ¦      _                                             |  +---+ +---+ +---+ +---+ +---+ +---+ 
//      +----------------------------------------------------+    75    74    73    72    11    5   
//        CAL RUN PRO                                           +---+ +---+ +---+ +---+ +---+ +---+ 
//                                                              |EXP| |y^x| | v~| |x^2| | ( | | ) | 
//                                                              +---+ +---+ +---+ +---+ +---+ +---+ 
//                                                                83    82    81    80    17    12  
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//  |CAL| |BAS| |BRK| |DEF| | v | | ^ | | < | | > | |SML| |SHI|  | 7 |  | 8 |  | 9 |  | / |  |X>M|  
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//    25    22    -     37    3     10    16    21    36    38     35     34     33     32     31   
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//  | Q | | W | | E | | R | | T | | Y | | U | | I | | O | | P |  | 4 |  | 5 |  | 6 |  | * |  | RM|  
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//    46    54    62    70    78     2     9    15    20    24     43     42     41     40     39   
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//  | A | | S | | D | | F | | G | | H | | J | | K | | L | | , |  | 1 |  | 2 |  | 3 |  | - |  | M+|  
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+  +---+  +---+  +---+  +---+  +---+  
//    45    53    61    69    77     1     8    14    19    23     51     50     49     48     47   
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---------+  +---+  +---+  +---+  +---+  +---+  
//  | Z | | X | | C | | V | | B | | N | | M | |SPC| |  ENTER  |  | 0 |  |+/-|  | . |  | + |  | = |  
//  +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---------+  +---+  +---+  +---+  +---+  +---+  
//    44    52    60    68    76     0     7    13       18        59     58     57     56     55   
//
byte keycode()
{
	
#asm
.equ REG_I	0x00		; index register
.equ REG_J	0x01		; index register
.equ REG_A	0x02		; accumulator
.equ REG_B	0x03		; accumulator
.equ REG_XL	0x04		; LSB of adress pointer
.equ REG_XH	0x05		; MSB of adress pointer
.equ REG_YL	0x06		; LSB of adress pointer
.equ REG_YH	0x07		; MSB of adress pointer
.equ REG_K	0x08		; counter
.equ REG_L	0x09		; counter
.equ REG_M	0x0A		; counter
.equ REG_N	0x0B		; counter

.equ KEY_P	0x3E00      ; address of key port

LIB_KEYC:	
	LIA  0			; 0->K (address)
	LP   REG_K
	EXAM
	LIB  7			; 7->B (increment)	
	LII  1			; 1->I (mask)

; ** Reading the keys from IA-port = 92 **
LIB_KEYC00E:	
	LP   REG_I
	LDM			    ; I -> A (assert P == REG_I)
	LIP  92			; A -> IA-Port
	EXAM
	OUTA
	WAIT 0x25
	INA				; IA-Port -> A
	CPIA 0			; if != 0,
LIB_KEYC01S:	
	JRNZP LIB_KEYC01E ; jump to keycode calculation
	LP   REG_B		; K = K + B
	LDM
	LP   REG_K
	ADM
	DECB			; B--
	LP   0   		; I = I >> 1
	LDM
	SL
	EXAM
	CPIA 32			; If I != 64 (i.e. old val. != 32),
LIB_KEYC00S:	
	JRNZM LIB_KEYC00E ; do the loop

; ** Reading the keys from K-port **
	LIA  28			; 28->K (address)
	LP   REG_K
	EXAM
	LII  1			; 1->I (mask)
LIB_KEYC02E:	
	LP   REG_I
	LDM				; I->A (assert P == REG_I)
	LIDP KEY_P		; Address of key port
	STD
	WAIT 0x25
	INA				; K-port -> A  
	CPIA 0			; if != 0,
LIB_KEYC03S:	
	JRNZP LIB_KEYC01E ; jump to keycode calculation
	LP   REG_K		; K = K - 8
	ADIM 8
	LP   REG_I		; I = I >> 1
	LDM
	SL
	EXAM
	CPIA 64			; If I != 128 (i.e. old val. != 64),
LIB_KEYC02S:	
	JRNZM LIB_KEYC02E		; do the loop
	LP   REG_K		; 255->K
    ORIM 0xFF
	LIA  128		; 128->A

; ** Key code calculation **
LIB_KEYC01E: 
	CPIA 128		; If A == 128, jump
	JRZP  LIB_KEYC04E ; to end (assert, A is never 0)
	RC
	SL				; A = A >> 1
	INCK			; K = K + 1			
LIB_KEYC05S:	
	JRM   LIB_KEYC01E ; do the loop

; ** Output & end **
LIB_KEYC04E:	
	LP   REG_K		; K->A , A used as return value
	LDM             ; return with value in A
LIB_KEYC06E:
	RTN
	#endasm

}

// keybrk ( )
// if BRK-key pressed (non blocking), return 1, else 0
byte keybrk ( ) {
	
#asm
LIB_KEYCL1S:    
	TEST 8				; Check for break-key
	JRNZP LIB_KEYCL1E	; If pressed...
	LIA 0
	RTN
LIB_KEYCL1E:
	LIA 1
	RTN
#endasm
	
}