; Test complicato PASM
.ORG 40000
; INCLUDE .\test_include.inc  ; rimosso per test
START:      LIA 0x10
            LIB 0x20
            STP 0x30
            LIA 2
            LIB 3
            CALL TEST_FUNC_A
            RTN

; Test di salti e chiamate
TEST_FUNC_A:
            LIA 0x05
            RTN


; etichette per memoria
TEST_LBL_1:
    .DB 0x05

TEST_LBL_2:
    .DB 0x2A, 0x4D, 0x2C, 0x0D, 0x2D, 0x08, 0xFF, 0xFF